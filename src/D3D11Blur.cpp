#include <D3D11Helper/D3D11Processing/D3D11Blur.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Helpers.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <sstream>
#include <utility>

namespace D3D11CoreLib {
namespace Processing {
namespace {

constexpr UINT kThreadGroupX = 16;
constexpr UINT kThreadGroupY = 16;
constexpr UINT kPackedWeightCount = 20;

struct D3D11BlurConstants {
    UINT srcWidth = 0;
    UINT srcHeight = 0;
    UINT dstWidth = 0;
    UINT dstHeight = 0;

    INT srcX = 0;
    INT srcY = 0;
    INT dstX = 0;
    INT dstY = 0;

    UINT radius = 0;
    UINT edgeMode = 0;
    UINT reserved0 = 0;
    UINT reserved1 = 0;

    float borderColor[4] = { 0, 0, 0, 0 };
    float weights[kPackedWeightCount] = {};
};
static_assert((sizeof(D3D11BlurConstants) % 16) == 0, "constant buffer size must be 16-byte aligned");
static_assert(sizeof(D3D11BlurConstants) <= 256, "D3D11ProcessingContext constant buffer is 256 bytes");

UINT DivideRoundUp(UINT value, UINT divisor) noexcept {
    return (value + divisor - 1u) / divisor;
}

DXGI_FORMAT ResolveFormat(DXGI_FORMAT requested, const D3D11Resource& resource) {
    return requested == DXGI_FORMAT_UNKNOWN ? resource.GetFormat() : requested;
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

std::array<float, kPackedWeightCount> MakeBlurWeights(const BlurDesc& desc) {
    if (desc.radius > D3D11Blurrer::MaxRadius) {
        std::ostringstream os;
        os << "D3D11Blurrer::DispatchBlur: radius must be <= " << D3D11Blurrer::MaxRadius;
        throw ValidationError(os.str());
    }

    std::array<float, kPackedWeightCount> weights = {};
    const UINT radius = desc.radius;

    if (radius == 0) {
        weights[0] = 1.0f;
        return weights;
    }

    switch (desc.mode) {
    case BlurMode::Box: {
        const float w = 1.0f / static_cast<float>(radius * 2u + 1u);
        for (UINT i = 0; i <= radius; ++i) {
            weights[i] = w;
        }
        return weights;
    }

    case BlurMode::Gaussian: {
        if (!(desc.sigma > 0.0f)) {
            throw ValidationError("D3D11Blurrer::DispatchBlur: sigma must be greater than zero for Gaussian blur");
        }

        float sum = 0.0f;
        for (UINT i = 0; i <= radius; ++i) {
            const float x = static_cast<float>(i);
            const float w = std::exp(-(x * x) / (2.0f * desc.sigma * desc.sigma));
            weights[i] = w;
            sum += (i == 0) ? w : (2.0f * w);
        }

        if (!(sum > 0.0f)) {
            throw ValidationError("D3D11Blurrer::DispatchBlur: invalid Gaussian weight sum");
        }

        for (UINT i = 0; i <= radius; ++i) {
            weights[i] /= sum;
        }
        return weights;
    }

    default:
        throw ValidationError("D3D11Blurrer::DispatchBlur: unsupported blur mode");
    }
}

D3D11BlurConstants MakeBlurConstants(
    const ProcessingRect& srcRect,
    const ProcessingRect& dstRect,
    const BlurDesc& desc,
    const std::array<float, kPackedWeightCount>& weights) {

    D3D11BlurConstants c = {};
    c.srcWidth = srcRect.width;
    c.srcHeight = srcRect.height;
    c.dstWidth = dstRect.width;
    c.dstHeight = dstRect.height;
    c.srcX = srcRect.x;
    c.srcY = srcRect.y;
    c.dstX = dstRect.x;
    c.dstY = dstRect.y;
    c.radius = desc.radius;
    c.edgeMode = static_cast<UINT>(desc.edgeMode);
    std::copy(desc.borderColor, desc.borderColor + 4, c.borderColor);
    std::copy(weights.begin(), weights.end(), c.weights);
    return c;
}

void BindAndDispatch(
    D3D11ProcessingContext& context,
    const D3D11ComputePipeline& pipeline,
    ID3D11DeviceContext* deviceContext,
    ID3D11ShaderResourceView* srv,
    ID3D11UnorderedAccessView* uav,
    const D3D11BlurConstants& constants) {

    if (!deviceContext) {
        throw ValidationError("D3D11Blurrer::DispatchBlur: null device context");
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

struct D3D11Blurrer::Pipelines {
    D3D11ComputePipeline horizontal;
    D3D11ComputePipeline vertical;
    bool initialized = false;
};

D3D11Blurrer::D3D11Blurrer() = default;
D3D11Blurrer::~D3D11Blurrer() = default;
D3D11Blurrer::D3D11Blurrer(D3D11Blurrer&&) noexcept = default;
D3D11Blurrer& D3D11Blurrer::operator=(D3D11Blurrer&&) noexcept = default;

void D3D11Blurrer::Initialize(D3D11ProcessingContext& context) {
    m_context = &context;
    m_shaderCache.Initialize(context);
    m_pipelines.reset();
}

void D3D11Blurrer::EnsureInitialized() const {
    if (!m_context) {
        throw ValidationError("D3D11Blurrer: blurrer is not initialized");
    }
}

void D3D11Blurrer::EnsurePipelines() {
    EnsureInitialized();

    if (!m_pipelines) {
        m_pipelines.reset(new Pipelines());
    }

    if (m_pipelines->initialized) {
        return;
    }

    auto* device = m_context->GetDevice();
    m_pipelines->horizontal.Initialize(device, m_shaderCache.GetComputeShader("GaussianBlurHorizontalRgba.hlsl"));
    m_pipelines->vertical.Initialize(device, m_shaderCache.GetComputeShader("GaussianBlurVerticalRgba.hlsl"));
    m_pipelines->initialized = true;
}

void D3D11Blurrer::DispatchBlur(
    ID3D11DeviceContext* deviceContext,
    D3D11Resource& src,
    D3D11Resource& scratch,
    D3D11Resource& dst,
    const BlurDesc& desc) {

    EnsurePipelines();

    constexpr const char* kFunction = "D3D11Blurrer::DispatchBlur";

    ValidateTexture2D(src, kFunction, "src");
    ValidateTexture2D(scratch, kFunction, "scratch");
    ValidateTexture2D(dst, kFunction, "dst");

    ValidateNotSameResource(src, scratch, kFunction);
    ValidateNotSameResource(src, dst, kFunction);
    ValidateNotSameResource(scratch, dst, kFunction);

    ValidateHasSrv(src, kFunction, "src");
    ValidateHasSrv(scratch, kFunction, "scratch");
    ValidateHasUav(scratch, kFunction, "scratch");
    ValidateHasUav(dst, kFunction, "dst");

    const DXGI_FORMAT srcFormat = ResolveFormat(DXGI_FORMAT_UNKNOWN, src);
    const DXGI_FORMAT scratchFormat = ResolveFormat(DXGI_FORMAT_UNKNOWN, scratch);
    const DXGI_FORMAT dstFormat = ResolveFormat(DXGI_FORMAT_UNKNOWN, dst);

    if (!IsRgbaLikeFormat(srcFormat) || !IsRgbaLikeFormat(scratchFormat) || !IsRgbaLikeFormat(dstFormat)) {
        throw UnsupportedFormatError("D3D11Blurrer::DispatchBlur: only RGBA-like formats are supported");
    }

    ValidateRgbaUavSupport(*m_context, scratchFormat, kFunction);
    ValidateRgbaUavSupport(*m_context, dstFormat, kFunction);

    const auto srcDesc = src.GetTexture2DDesc();
    const auto scratchDesc = scratch.GetTexture2DDesc();
    const auto dstDesc = dst.GetTexture2DDesc();

    const ProcessingRect srcRect = ResolveRect(desc.srcRect, srcDesc.Width, srcDesc.Height);
    const ProcessingRect dstRect = ResolveRect(desc.dstRect, dstDesc.Width, dstDesc.Height);

    ValidateRectInside(srcRect, srcDesc.Width, srcDesc.Height, kFunction, "srcRect");
    ValidateRectInside(dstRect, dstDesc.Width, dstDesc.Height, kFunction, "dstRect");

    if (srcRect.width != dstRect.width || srcRect.height != dstRect.height) {
        throw ValidationError("D3D11Blurrer::DispatchBlur: blur does not resize; srcRect and dstRect sizes must match");
    }

    if (scratchDesc.Width < dstRect.width || scratchDesc.Height < dstRect.height) {
        throw ValidationError("D3D11Blurrer::DispatchBlur: scratch texture is smaller than processing rect");
    }

    const auto weights = MakeBlurWeights(desc);

    auto srcView = CreateRgbaTextureViewSet(*m_context, src, true, false, srcFormat);
    auto scratchWriteView = CreateRgbaTextureViewSet(*m_context, scratch, false, true, scratchFormat);

    const ProcessingRect scratchRect = { 0, 0, dstRect.width, dstRect.height };

    auto horizontalConstants = MakeBlurConstants(srcRect, scratchRect, desc, weights);
    BindAndDispatch(
        *m_context,
        m_pipelines->horizontal,
        deviceContext,
        srcView.srv.Get(),
        scratchWriteView.uav.Get(),
        horizontalConstants);

    auto scratchReadView = CreateRgbaTextureViewSet(*m_context, scratch, true, false, scratchFormat);
    auto dstView = CreateRgbaTextureViewSet(*m_context, dst, false, true, dstFormat);

    auto verticalConstants = MakeBlurConstants(scratchRect, dstRect, desc, weights);
    BindAndDispatch(
        *m_context,
        m_pipelines->vertical,
        deviceContext,
        scratchReadView.srv.Get(),
        dstView.uav.Get(),
        verticalConstants);
}

D3D11Resource D3D11Blurrer::CreateOutputTexture(
    D3D11Core& core,
    UINT width,
    UINT height,
    DXGI_FORMAT format) {

    EnsureInitialized();

    if (width == 0 || height == 0) {
        throw ValidationError("D3D11Blurrer::CreateOutputTexture: size is zero");
    }

    if (!IsRgbaLikeFormat(format)) {
        throw UnsupportedFormatError("D3D11Blurrer::CreateOutputTexture: only RGBA-like formats are supported");
    }

    ValidateRgbaUavSupport(*m_context, format, "D3D11Blurrer::CreateOutputTexture");

    return CreateTexture2D(
        core,
        width,
        height,
        format,
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
}

D3D11Resource D3D11Blurrer::CreateScratchTexture(
    D3D11Core& core,
    UINT width,
    UINT height,
    DXGI_FORMAT format) {

    EnsureInitialized();

    if (width == 0 || height == 0) {
        throw ValidationError("D3D11Blurrer::CreateScratchTexture: size is zero");
    }

    if (!IsRgbaLikeFormat(format)) {
        throw UnsupportedFormatError("D3D11Blurrer::CreateScratchTexture: only RGBA-like formats are supported");
    }

    ValidateRgbaUavSupport(*m_context, format, "D3D11Blurrer::CreateScratchTexture");

    return CreateTexture2D(
        core,
        width,
        height,
        format,
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
}

} // namespace Processing
} // namespace D3D11CoreLib
