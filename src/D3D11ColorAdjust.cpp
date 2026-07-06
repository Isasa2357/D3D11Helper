#include <D3D11Helper/D3D11Processing/D3D11ColorAdjust.hpp>
#include <D3D11Helper/D3D11Framework/D3D11Helpers.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

namespace D3D11CoreLib {
namespace Processing {
namespace {

constexpr UINT kThreadGroupX = 16;
constexpr UINT kThreadGroupY = 16;

struct D3D11ColorAdjustConstants {
    UINT srcWidth = 0;
    UINT srcHeight = 0;
    UINT dstWidth = 0;
    UINT dstHeight = 0;

    INT srcX = 0;
    INT srcY = 0;
    INT dstX = 0;
    INT dstY = 0;

    float brightness = 0.0f;
    float contrast = 1.0f;
    float gamma = 1.0f;
    float saturation = 1.0f;
};
static_assert((sizeof(D3D11ColorAdjustConstants) % 16) == 0, "constant buffer size must be 16-byte aligned");
static_assert(sizeof(D3D11ColorAdjustConstants) <= 256, "D3D11ProcessingContext constant buffer is 256 bytes");

UINT DivideRoundUp(UINT value, UINT divisor) noexcept {
    return (value + divisor - 1u) / divisor;
}

DXGI_FORMAT ResolveFormat(DXGI_FORMAT requested, const D3D11Resource& resource) {
    return requested == DXGI_FORMAT_UNKNOWN ? resource.GetFormat() : requested;
}

bool IsFinite(float v) noexcept {
    return std::isfinite(v) != 0;
}

void ValidateFinite(float v, const char* functionName, const char* fieldName) {
    if (!IsFinite(v)) {
        std::ostringstream os;
        os << functionName << ": " << fieldName << " must be finite";
        throw ValidationError(os.str());
    }
}

void ValidateTexture2D(const D3D11Resource& resource, const char* functionName, const char* argumentName) {
    if (!resource.Get()) {
        std::ostringstream os;
        os << functionName << ": " << argumentName << " is null";
        throw ValidationError(os.str());
    }

    const auto desc = resource.GetTexture2DDesc();
    if (desc.Width == 0 || desc.Height == 0) {
        std::ostringstream os;
        os << functionName << ": " << argumentName << " is not Texture2D or has zero size";
        throw ValidationError(os.str());
    }
}

void ValidateHasSrv(const D3D11Resource& resource, const char* functionName, const char* argumentName) {
    const auto desc = resource.GetTexture2DDesc();
    if ((desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) == 0) {
        std::ostringstream os;
        os << functionName << ": " << argumentName << " must have D3D11_BIND_SHADER_RESOURCE";
        throw ValidationError(os.str());
    }
}

void ValidateHasUav(const D3D11Resource& resource, const char* functionName, const char* argumentName) {
    const auto desc = resource.GetTexture2DDesc();
    if ((desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) == 0) {
        std::ostringstream os;
        os << functionName << ": " << argumentName << " must have D3D11_BIND_UNORDERED_ACCESS";
        throw ValidationError(os.str());
    }
}

void ValidateNotSameResource(const D3D11Resource& a, const D3D11Resource& b, const char* functionName) {
    if (a.Get() == b.Get()) {
        std::ostringstream os;
        os << functionName << ": in-place processing is not supported";
        throw ValidationError(os.str());
    }
}

void ValidateRgbaUavSupport(D3D11ProcessingContext& context, DXGI_FORMAT format, const char* functionName) {
    if (format == DXGI_FORMAT_R8G8B8A8_UNORM && !context.SupportsRgba8Uav()) {
        throw UnsupportedFeatureError(std::string(functionName) + ": R8G8B8A8 UAV is not supported");
    }
    if (format == DXGI_FORMAT_B8G8R8A8_UNORM && !context.SupportsBgra8Uav()) {
        throw UnsupportedFeatureError(std::string(functionName) + ": B8G8R8A8 UAV is not supported");
    }
    if (format == DXGI_FORMAT_R16G16B16A16_FLOAT && !context.SupportsRgba16FloatUav()) {
        throw UnsupportedFeatureError(std::string(functionName) + ": RGBA16F UAV is not supported");
    }
}

void ValidateColorAdjustDesc(const ColorAdjustDesc& desc, const char* functionName) {
    ValidateFinite(desc.brightness, functionName, "brightness");
    ValidateFinite(desc.contrast, functionName, "contrast");
    ValidateFinite(desc.gamma, functionName, "gamma");
    ValidateFinite(desc.saturation, functionName, "saturation");

    if (desc.contrast < 0.0f) {
        throw ValidationError("D3D11ColorAdjuster::DispatchColorAdjust: contrast must be >= 0");
    }
    if (!(desc.gamma > 0.0f)) {
        throw ValidationError("D3D11ColorAdjuster::DispatchColorAdjust: gamma must be > 0");
    }
    if (desc.saturation < 0.0f) {
        throw ValidationError("D3D11ColorAdjuster::DispatchColorAdjust: saturation must be >= 0");
    }
}

D3D11ColorAdjustConstants MakeColorAdjustConstants(
    const ProcessingRect& srcRect,
    const ProcessingRect& dstRect,
    const ColorAdjustDesc& desc) {

    D3D11ColorAdjustConstants c = {};
    c.srcWidth = srcRect.width;
    c.srcHeight = srcRect.height;
    c.dstWidth = dstRect.width;
    c.dstHeight = dstRect.height;
    c.srcX = srcRect.x;
    c.srcY = srcRect.y;
    c.dstX = dstRect.x;
    c.dstY = dstRect.y;
    c.brightness = desc.brightness;
    c.contrast = desc.contrast;
    c.gamma = desc.gamma;
    c.saturation = desc.saturation;
    return c;
}

void BindAndDispatch(
    D3D11ProcessingContext& context,
    const D3D11ComputePipeline& pipeline,
    ID3D11DeviceContext* deviceContext,
    ID3D11ShaderResourceView* srv,
    ID3D11UnorderedAccessView* uav,
    const D3D11ColorAdjustConstants& constants) {

    if (!deviceContext) {
        throw ValidationError("D3D11ColorAdjuster::DispatchColorAdjust: null device context");
    }

    context.UpdateConstants(deviceContext, &constants, static_cast<UINT>(sizeof(constants)));

    ID3D11Buffer* cb = context.ConstantBuffer();
    deviceContext->CSSetConstantBuffers(0, 1, &cb);

    ID3D11ShaderResourceView* srvs[] = { srv };
    deviceContext->CSSetShaderResources(0, 1, srvs);

    UINT initialCounts[] = { static_cast<UINT>(-1) };
    ID3D11UnorderedAccessView* uavs[] = { uav };
    deviceContext->CSSetUnorderedAccessViews(0, 1, uavs, initialCounts);

    pipeline.Bind(deviceContext);
    deviceContext->Dispatch(
        DivideRoundUp(constants.dstWidth, kThreadGroupX),
        DivideRoundUp(constants.dstHeight, kThreadGroupY),
        1);

    ID3D11ShaderResourceView* nullSrvs[] = { nullptr };
    ID3D11UnorderedAccessView* nullUavs[] = { nullptr };
    ID3D11Buffer* nullCb = nullptr;
    deviceContext->CSSetShaderResources(0, 1, nullSrvs);
    deviceContext->CSSetUnorderedAccessViews(0, 1, nullUavs, nullptr);
    deviceContext->CSSetConstantBuffers(0, 1, &nullCb);
    pipeline.Unbind(deviceContext);
}

} // namespace

struct D3D11ColorAdjuster::Pipelines {
    D3D11ComputePipeline colorAdjust;
    bool initialized = false;
};

D3D11ColorAdjuster::D3D11ColorAdjuster() = default;
D3D11ColorAdjuster::~D3D11ColorAdjuster() = default;
D3D11ColorAdjuster::D3D11ColorAdjuster(D3D11ColorAdjuster&&) noexcept = default;
D3D11ColorAdjuster& D3D11ColorAdjuster::operator=(D3D11ColorAdjuster&&) noexcept = default;

void D3D11ColorAdjuster::Initialize(D3D11ProcessingContext& context) {
    m_context = &context;
    m_shaderCache.Initialize(context);
    m_pipelines.reset();
}

void D3D11ColorAdjuster::EnsureInitialized() const {
    if (!m_context) {
        throw ValidationError("D3D11ColorAdjuster: adjuster is not initialized");
    }
}

void D3D11ColorAdjuster::EnsurePipelines() {
    EnsureInitialized();

    if (!m_pipelines) {
        m_pipelines.reset(new Pipelines());
    }

    if (m_pipelines->initialized) {
        return;
    }

    m_pipelines->colorAdjust.Initialize(m_context->GetDevice(), m_shaderCache.GetComputeShader("ColorAdjustRgba.hlsl"));
    m_pipelines->initialized = true;
}

void D3D11ColorAdjuster::DispatchColorAdjust(
    ID3D11DeviceContext* deviceContext,
    D3D11Resource& src,
    D3D11Resource& dst,
    const ColorAdjustDesc& desc) {

    EnsurePipelines();

    constexpr const char* kFunction = "D3D11ColorAdjuster::DispatchColorAdjust";

    ValidateTexture2D(src, kFunction, "src");
    ValidateTexture2D(dst, kFunction, "dst");
    ValidateNotSameResource(src, dst, kFunction);
    ValidateHasSrv(src, kFunction, "src");
    ValidateHasUav(dst, kFunction, "dst");
    ValidateColorAdjustDesc(desc, kFunction);

    const DXGI_FORMAT srcFormat = ResolveFormat(desc.srcFormat, src);
    const DXGI_FORMAT dstFormat = ResolveFormat(desc.dstFormat, dst);

    if (!IsRgbaLikeFormat(srcFormat) || !IsRgbaLikeFormat(dstFormat)) {
        throw UnsupportedFormatError("D3D11ColorAdjuster::DispatchColorAdjust: only RGBA-like formats are supported");
    }

    ValidateRgbaUavSupport(*m_context, dstFormat, kFunction);

    const auto srcDesc = src.GetTexture2DDesc();
    const auto dstDesc = dst.GetTexture2DDesc();

    const ProcessingRect srcRect = ResolveRect(desc.srcRect, srcDesc.Width, srcDesc.Height);
    const ProcessingRect dstRect = ResolveRect(desc.dstRect, dstDesc.Width, dstDesc.Height);

    ValidateRectInside(srcRect, srcDesc.Width, srcDesc.Height, kFunction, "srcRect");
    ValidateRectInside(dstRect, dstDesc.Width, dstDesc.Height, kFunction, "dstRect");

    if (srcRect.width != dstRect.width || srcRect.height != dstRect.height) {
        throw ValidationError("D3D11ColorAdjuster::DispatchColorAdjust: color adjustment does not resize; srcRect and dstRect sizes must match");
    }

    const auto constants = MakeColorAdjustConstants(srcRect, dstRect, desc);

    auto srcView = CreateRgbaTextureViewSet(*m_context, src, true, false, srcFormat);
    auto dstView = CreateRgbaTextureViewSet(*m_context, dst, false, true, dstFormat);

    BindAndDispatch(
        *m_context,
        m_pipelines->colorAdjust,
        deviceContext,
        srcView.srv.Get(),
        dstView.uav.Get(),
        constants);
}

D3D11Resource D3D11ColorAdjuster::CreateOutputTexture(
    D3D11Core& core,
    UINT width,
    UINT height,
    DXGI_FORMAT format) {

    EnsureInitialized();

    if (width == 0 || height == 0) {
        throw ValidationError("D3D11ColorAdjuster::CreateOutputTexture: size is zero");
    }

    if (!IsRgbaLikeFormat(format)) {
        throw UnsupportedFormatError("D3D11ColorAdjuster::CreateOutputTexture: only RGBA-like formats are supported");
    }

    ValidateRgbaUavSupport(*m_context, format, "D3D11ColorAdjuster::CreateOutputTexture");

    return CreateTexture2D(
        core,
        width,
        height,
        format,
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
}

} // namespace Processing
} // namespace D3D11CoreLib
