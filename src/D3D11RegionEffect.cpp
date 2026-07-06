#include <D3D11Helper/D3D11Processing/D3D11RegionEffect.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Helpers.hpp>

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

struct D3D11RegionEffectConstants {
    UINT srcWidth = 0;
    UINT srcHeight = 0;
    UINT dstWidth = 0;
    UINT dstHeight = 0;

    INT srcX = 0;
    INT srcY = 0;
    INT dstX = 0;
    INT dstY = 0;

    UINT shape = 0;
    UINT effect = 0;
    UINT selection = 0;
    UINT reserved0 = 0;

    float centerX = 0.0f;
    float centerY = 0.0f;
    float radius = 1.0f;
    float edgeSoftness = 0.0f;

    float rectX = 0.0f;
    float rectY = 0.0f;
    float rectWidth = 1.0f;
    float rectHeight = 1.0f;

    float darkenFactor = 0.5f;
    float tintStrength = 0.25f;
    float grayscaleStrength = 1.0f;
    float highlightStrength = 0.25f;

    float vignetteStrength = 1.0f;
    float alphaFactor = 0.0f;
    float reserved1 = 0.0f;
    float reserved2 = 0.0f;

    float tintColor[4] = { 1, 1, 1, 1 };
    float highlightColor[4] = { 1, 1, 1, 1 };
};
static_assert((sizeof(D3D11RegionEffectConstants) % 16) == 0, "constant buffer size must be 16-byte aligned");
static_assert(sizeof(D3D11RegionEffectConstants) <= 256, "D3D11ProcessingContext constant buffer is 256 bytes");

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

void ValidateRange01(float v, const char* functionName, const char* fieldName) {
    ValidateFinite(v, functionName, fieldName);
    if (v < 0.0f || v > 1.0f) {
        std::ostringstream os;
        os << functionName << ": " << fieldName << " must be in [0, 1]";
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

void ValidateRegionDesc(const RegionEffectDesc& desc, const char* functionName) {
    ValidateFinite(desc.centerX, functionName, "centerX");
    ValidateFinite(desc.centerY, functionName, "centerY");
    ValidateFinite(desc.radius, functionName, "radius");
    ValidateFinite(desc.rectX, functionName, "rectX");
    ValidateFinite(desc.rectY, functionName, "rectY");
    ValidateFinite(desc.rectWidth, functionName, "rectWidth");
    ValidateFinite(desc.rectHeight, functionName, "rectHeight");
    ValidateFinite(desc.edgeSoftness, functionName, "edgeSoftness");

    if (desc.edgeSoftness < 0.0f) {
        throw ValidationError("D3D11RegionEffect::DispatchRegionEffect: edgeSoftness must be >= 0");
    }

    switch (desc.shape) {
    case RegionShape::Circle:
        if (!(desc.radius > 0.0f)) {
            throw ValidationError("D3D11RegionEffect::DispatchRegionEffect: radius must be > 0 for circle region");
        }
        break;
    case RegionShape::Rect:
        if (!(desc.rectWidth > 0.0f) || !(desc.rectHeight > 0.0f)) {
            throw ValidationError("D3D11RegionEffect::DispatchRegionEffect: rectWidth and rectHeight must be > 0 for rect region");
        }
        break;
    default:
        throw ValidationError("D3D11RegionEffect::DispatchRegionEffect: unsupported region shape");
    }

    switch (desc.selection) {
    case RegionSelection::Inside:
    case RegionSelection::Outside:
        break;
    default:
        throw ValidationError("D3D11RegionEffect::DispatchRegionEffect: unsupported region selection");
    }

    switch (desc.effect) {
    case RegionEffectMode::Copy:
    case RegionEffectMode::Darken:
    case RegionEffectMode::Tint:
    case RegionEffectMode::Grayscale:
    case RegionEffectMode::Highlight:
    case RegionEffectMode::AlphaFade:
    case RegionEffectMode::Vignette:
        break;
    default:
        throw ValidationError("D3D11RegionEffect::DispatchRegionEffect: unsupported region effect mode");
    }

    ValidateRange01(desc.darkenFactor, functionName, "darkenFactor");
    ValidateRange01(desc.tintStrength, functionName, "tintStrength");
    ValidateRange01(desc.grayscaleStrength, functionName, "grayscaleStrength");
    ValidateRange01(desc.highlightStrength, functionName, "highlightStrength");
    ValidateRange01(desc.alphaFactor, functionName, "alphaFactor");
    ValidateRange01(desc.vignetteStrength, functionName, "vignetteStrength");

    for (int i = 0; i < 4; ++i) {
        ValidateRange01(desc.tintColor[i], functionName, "tintColor");
        ValidateRange01(desc.highlightColor[i], functionName, "highlightColor");
    }
}

D3D11RegionEffectConstants MakeRegionConstants(
    const ProcessingRect& srcRect,
    const ProcessingRect& dstRect,
    const RegionEffectDesc& desc) {

    D3D11RegionEffectConstants c = {};
    c.srcWidth = srcRect.width;
    c.srcHeight = srcRect.height;
    c.dstWidth = dstRect.width;
    c.dstHeight = dstRect.height;
    c.srcX = srcRect.x;
    c.srcY = srcRect.y;
    c.dstX = dstRect.x;
    c.dstY = dstRect.y;

    c.shape = static_cast<UINT>(desc.shape);
    c.effect = static_cast<UINT>(desc.effect);
    c.selection = static_cast<UINT>(desc.selection);

    c.centerX = desc.centerX;
    c.centerY = desc.centerY;
    c.radius = desc.radius;
    c.edgeSoftness = desc.edgeSoftness;

    c.rectX = desc.rectX;
    c.rectY = desc.rectY;
    c.rectWidth = desc.rectWidth;
    c.rectHeight = desc.rectHeight;

    c.darkenFactor = desc.darkenFactor;
    c.tintStrength = desc.tintStrength;
    c.grayscaleStrength = desc.grayscaleStrength;
    c.highlightStrength = desc.highlightStrength;
    c.vignetteStrength = desc.vignetteStrength;
    c.alphaFactor = desc.alphaFactor;

    std::copy(desc.tintColor, desc.tintColor + 4, c.tintColor);
    std::copy(desc.highlightColor, desc.highlightColor + 4, c.highlightColor);
    return c;
}

void BindAndDispatch(
    D3D11ProcessingContext& context,
    const D3D11ComputePipeline& pipeline,
    ID3D11DeviceContext* deviceContext,
    ID3D11ShaderResourceView* srv,
    ID3D11UnorderedAccessView* uav,
    const D3D11RegionEffectConstants& constants) {

    if (!deviceContext) {
        throw ValidationError("D3D11RegionEffect::DispatchRegionEffect: null device context");
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

struct D3D11RegionEffect::Pipelines {
    D3D11ComputePipeline regionEffect;
    bool initialized = false;
};

D3D11RegionEffect::D3D11RegionEffect() = default;
D3D11RegionEffect::~D3D11RegionEffect() = default;
D3D11RegionEffect::D3D11RegionEffect(D3D11RegionEffect&&) noexcept = default;
D3D11RegionEffect& D3D11RegionEffect::operator=(D3D11RegionEffect&&) noexcept = default;

void D3D11RegionEffect::Initialize(D3D11ProcessingContext& context) {
    m_context = &context;
    m_shaderCache.Initialize(context);
    m_pipelines.reset();
}

void D3D11RegionEffect::EnsureInitialized() const {
    if (!m_context) {
        throw ValidationError("D3D11RegionEffect: processor is not initialized");
    }
}

void D3D11RegionEffect::EnsurePipelines() {
    EnsureInitialized();

    if (!m_pipelines) {
        m_pipelines.reset(new Pipelines());
    }

    if (m_pipelines->initialized) {
        return;
    }

    m_pipelines->regionEffect.Initialize(
        m_context->GetDevice(),
        m_shaderCache.GetComputeShader("RegionEffectRgba.hlsl"));
    m_pipelines->initialized = true;
}

void D3D11RegionEffect::DispatchRegionEffect(
    ID3D11DeviceContext* deviceContext,
    D3D11Resource& src,
    D3D11Resource& dst,
    const RegionEffectDesc& desc) {

    EnsurePipelines();

    constexpr const char* kFunction = "D3D11RegionEffect::DispatchRegionEffect";

    ValidateTexture2D(src, kFunction, "src");
    ValidateTexture2D(dst, kFunction, "dst");
    ValidateNotSameResource(src, dst, kFunction);
    ValidateHasSrv(src, kFunction, "src");
    ValidateHasUav(dst, kFunction, "dst");
    ValidateRegionDesc(desc, kFunction);

    const DXGI_FORMAT srcFormat = ResolveFormat(desc.srcFormat, src);
    const DXGI_FORMAT dstFormat = ResolveFormat(desc.dstFormat, dst);

    if (!IsRgbaLikeFormat(srcFormat) || !IsRgbaLikeFormat(dstFormat)) {
        throw UnsupportedFormatError("D3D11RegionEffect::DispatchRegionEffect: only RGBA-like formats are supported");
    }

    ValidateRgbaUavSupport(*m_context, dstFormat, kFunction);

    const auto srcDesc = src.GetTexture2DDesc();
    const auto dstDesc = dst.GetTexture2DDesc();

    const ProcessingRect srcRect = ResolveRect(desc.srcRect, srcDesc.Width, srcDesc.Height);
    const ProcessingRect dstRect = ResolveRect(desc.dstRect, dstDesc.Width, dstDesc.Height);

    ValidateRectInside(srcRect, srcDesc.Width, srcDesc.Height, kFunction, "srcRect");
    ValidateRectInside(dstRect, dstDesc.Width, dstDesc.Height, kFunction, "dstRect");

    if (srcRect.width != dstRect.width || srcRect.height != dstRect.height) {
        throw ValidationError("D3D11RegionEffect::DispatchRegionEffect: region effect does not resize; srcRect and dstRect sizes must match");
    }

    auto srcView = CreateRgbaTextureViewSet(*m_context, src, true, false, srcFormat);
    auto dstView = CreateRgbaTextureViewSet(*m_context, dst, false, true, dstFormat);
    const auto constants = MakeRegionConstants(srcRect, dstRect, desc);

    BindAndDispatch(
        *m_context,
        m_pipelines->regionEffect,
        deviceContext,
        srcView.srv.Get(),
        dstView.uav.Get(),
        constants);
}

D3D11Resource D3D11RegionEffect::CreateOutputTexture(
    D3D11Core& core,
    UINT width,
    UINT height,
    DXGI_FORMAT format) {

    EnsureInitialized();

    if (width == 0 || height == 0) {
        throw ValidationError("D3D11RegionEffect::CreateOutputTexture: size is zero");
    }

    if (!IsRgbaLikeFormat(format)) {
        throw UnsupportedFormatError("D3D11RegionEffect::CreateOutputTexture: only RGBA-like formats are supported");
    }

    ValidateRgbaUavSupport(*m_context, format, "D3D11RegionEffect::CreateOutputTexture");

    return CreateTexture2D(
        core,
        width,
        height,
        format,
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
}

} // namespace Processing
} // namespace D3D11CoreLib
