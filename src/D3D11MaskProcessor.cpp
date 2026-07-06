#include <D3D11Helper/D3D11Processing/D3D11MaskProcessor.hpp>
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

struct D3D11MaskConstants {
    UINT width = 0;
    UINT height = 0;
    INT srcX = 0;
    INT srcY = 0;

    INT maskX = 0;
    INT maskY = 0;
    INT dstX = 0;
    INT dstY = 0;

    INT overlayX = 0;
    INT overlayY = 0;
    UINT mode = 0;
    UINT channel = 0;

    UINT channelB = 0;
    UINT invert = 0;
    UINT invertB = 0;
    UINT reserved0 = 0;

    float strength = 1.0f;
    float opacity = 1.0f;
    float scale = 1.0f;
    float bias = 0.0f;
};
static_assert((sizeof(D3D11MaskConstants) % 16) == 0, "constant buffer size must be 16-byte aligned");
static_assert(sizeof(D3D11MaskConstants) <= 256, "D3D11ProcessingContext constant buffer is 256 bytes");

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

void ValidateRgbaLike(DXGI_FORMAT format, const char* functionName, const char* argumentName) {
    if (!IsRgbaLikeFormat(format)) {
        std::ostringstream os;
        os << functionName << ": " << argumentName << " must be RGBA-like format";
        throw UnsupportedFormatError(os.str());
    }
}

void ValidateChannel(MaskChannel channel, const char* functionName) {
    switch (channel) {
    case MaskChannel::Red:
    case MaskChannel::Green:
    case MaskChannel::Blue:
    case MaskChannel::Alpha:
    case MaskChannel::Luma:
        return;
    default:
        throw ValidationError(std::string(functionName) + ": unsupported mask channel");
    }
}

void ValidateSameSize(const ProcessingRect& a, const ProcessingRect& b, const char* functionName, const char* aName, const char* bName) {
    if (a.width != b.width || a.height != b.height) {
        std::ostringstream os;
        os << functionName << ": " << aName << " and " << bName << " sizes must match";
        throw ValidationError(os.str());
    }
}

D3D11MaskConstants MakeCommonConstants(UINT width, UINT height, const ProcessingRect& src, const ProcessingRect& mask, const ProcessingRect& dst) {
    D3D11MaskConstants c = {};
    c.width = width;
    c.height = height;
    c.srcX = src.x;
    c.srcY = src.y;
    c.maskX = mask.x;
    c.maskY = mask.y;
    c.dstX = dst.x;
    c.dstY = dst.y;
    return c;
}

void BindAndDispatch(
    D3D11ProcessingContext& context,
    const D3D11ComputePipeline& pipeline,
    ID3D11DeviceContext* deviceContext,
    ID3D11ShaderResourceView* const* srvs,
    UINT srvCount,
    ID3D11UnorderedAccessView* uav,
    const D3D11MaskConstants& constants,
    const char* functionName) {

    if (!deviceContext) {
        throw ValidationError(std::string(functionName) + ": null device context");
    }

    context.UpdateConstants(deviceContext, &constants, static_cast<UINT>(sizeof(constants)));

    ID3D11Buffer* cb = context.ConstantBuffer();
    deviceContext->CSSetConstantBuffers(0, 1, &cb);
    deviceContext->CSSetShaderResources(0, srvCount, srvs);

    UINT initialCounts[] = { static_cast<UINT>(-1) };
    ID3D11UnorderedAccessView* uavs[] = { uav };
    deviceContext->CSSetUnorderedAccessViews(0, 1, uavs, initialCounts);

    pipeline.Bind(deviceContext);
    deviceContext->Dispatch(DivideRoundUp(constants.width, kThreadGroupX), DivideRoundUp(constants.height, kThreadGroupY), 1);

    ID3D11ShaderResourceView* nullSrvs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
    ID3D11UnorderedAccessView* nullUavs[] = { nullptr };
    ID3D11Buffer* nullCb = nullptr;
    deviceContext->CSSetShaderResources(0, srvCount, nullSrvs);
    deviceContext->CSSetUnorderedAccessViews(0, 1, nullUavs, nullptr);
    deviceContext->CSSetConstantBuffers(0, 1, &nullCb);
    pipeline.Unbind(deviceContext);
}

} // namespace

struct D3D11MaskProcessor::Pipelines {
    D3D11ComputePipeline apply;
    D3D11ComputePipeline blend;
    D3D11ComputePipeline combine;
    D3D11ComputePipeline invert;
    bool initialized = false;
};

D3D11MaskProcessor::D3D11MaskProcessor() = default;
D3D11MaskProcessor::~D3D11MaskProcessor() = default;
D3D11MaskProcessor::D3D11MaskProcessor(D3D11MaskProcessor&&) noexcept = default;
D3D11MaskProcessor& D3D11MaskProcessor::operator=(D3D11MaskProcessor&&) noexcept = default;

void D3D11MaskProcessor::Initialize(D3D11ProcessingContext& context) {
    m_context = &context;
    m_shaderCache.Initialize(context);
    m_pipelines.reset();
}

void D3D11MaskProcessor::EnsureInitialized() const {
    if (!m_context) {
        throw ValidationError("D3D11MaskProcessor: processor is not initialized");
    }
}

void D3D11MaskProcessor::EnsurePipelines() {
    EnsureInitialized();
    if (!m_pipelines) {
        m_pipelines.reset(new Pipelines());
    }
    if (m_pipelines->initialized) {
        return;
    }

    auto* device = m_context->GetDevice();
    m_pipelines->apply.Initialize(device, m_shaderCache.GetComputeShader("MaskApplyRgba.hlsl"));
    m_pipelines->blend.Initialize(device, m_shaderCache.GetComputeShader("MaskBlendRgba.hlsl"));
    m_pipelines->combine.Initialize(device, m_shaderCache.GetComputeShader("MaskCombineRgba.hlsl"));
    m_pipelines->invert.Initialize(device, m_shaderCache.GetComputeShader("MaskInvertRgba.hlsl"));
    m_pipelines->initialized = true;
}

void D3D11MaskProcessor::DispatchApplyMask(
    ID3D11DeviceContext* deviceContext,
    D3D11Resource& src,
    D3D11Resource& mask,
    D3D11Resource& dst,
    const MaskApplyDesc& desc) {

    EnsurePipelines();
    constexpr const char* kFunction = "D3D11MaskProcessor::DispatchApplyMask";

    ValidateTexture2D(src, kFunction, "src");
    ValidateTexture2D(mask, kFunction, "mask");
    ValidateTexture2D(dst, kFunction, "dst");
    ValidateNotSameResource(src, dst, kFunction);
    ValidateNotSameResource(mask, dst, kFunction);
    ValidateHasSrv(src, kFunction, "src");
    ValidateHasSrv(mask, kFunction, "mask");
    ValidateHasUav(dst, kFunction, "dst");
    ValidateChannel(desc.channel, kFunction);
    ValidateFinite(desc.strength, kFunction, "strength");

    switch (desc.mode) {
    case MaskApplyMode::ApplyAlpha:
    case MaskApplyMode::MultiplyRgb:
    case MaskApplyMode::MultiplyRgba:
    case MaskApplyMode::ReplaceAlpha:
        break;
    default:
        throw ValidationError("D3D11MaskProcessor::DispatchApplyMask: unsupported mask apply mode");
    }

    const DXGI_FORMAT srcFormat = ResolveFormat(desc.srcFormat, src);
    const DXGI_FORMAT maskFormat = ResolveFormat(desc.maskFormat, mask);
    const DXGI_FORMAT dstFormat = ResolveFormat(desc.dstFormat, dst);
    ValidateRgbaLike(srcFormat, kFunction, "srcFormat");
    ValidateRgbaLike(maskFormat, kFunction, "maskFormat");
    ValidateRgbaLike(dstFormat, kFunction, "dstFormat");
    ValidateRgbaUavSupport(*m_context, dstFormat, kFunction);

    const auto sd = src.GetTexture2DDesc();
    const auto md = mask.GetTexture2DDesc();
    const auto dd = dst.GetTexture2DDesc();
    const ProcessingRect sr = ResolveRect(desc.srcRect, sd.Width, sd.Height);
    const ProcessingRect mr = ResolveRect(desc.maskRect, md.Width, md.Height);
    const ProcessingRect dr = ResolveRect(desc.dstRect, dd.Width, dd.Height);
    ValidateRectInside(sr, sd.Width, sd.Height, kFunction, "srcRect");
    ValidateRectInside(mr, md.Width, md.Height, kFunction, "maskRect");
    ValidateRectInside(dr, dd.Width, dd.Height, kFunction, "dstRect");
    ValidateSameSize(sr, dr, kFunction, "srcRect", "dstRect");
    ValidateSameSize(mr, dr, kFunction, "maskRect", "dstRect");

    auto sv = CreateRgbaTextureViewSet(*m_context, src, true, false, srcFormat);
    auto mv = CreateRgbaTextureViewSet(*m_context, mask, true, false, maskFormat);
    auto dv = CreateRgbaTextureViewSet(*m_context, dst, false, true, dstFormat);

    D3D11MaskConstants c = MakeCommonConstants(dr.width, dr.height, sr, mr, dr);
    c.mode = static_cast<UINT>(desc.mode);
    c.channel = static_cast<UINT>(desc.channel);
    c.invert = desc.invert ? 1u : 0u;
    c.strength = desc.strength;

    ID3D11ShaderResourceView* srvs[] = { sv.srv.Get(), mv.srv.Get() };
    BindAndDispatch(*m_context, m_pipelines->apply, deviceContext, srvs, 2, dv.uav.Get(), c, kFunction);
}

void D3D11MaskProcessor::DispatchBlendByMask(
    ID3D11DeviceContext* deviceContext,
    D3D11Resource& base,
    D3D11Resource& overlay,
    D3D11Resource& mask,
    D3D11Resource& dst,
    const MaskBlendDesc& desc) {

    EnsurePipelines();
    constexpr const char* kFunction = "D3D11MaskProcessor::DispatchBlendByMask";

    ValidateTexture2D(base, kFunction, "base");
    ValidateTexture2D(overlay, kFunction, "overlay");
    ValidateTexture2D(mask, kFunction, "mask");
    ValidateTexture2D(dst, kFunction, "dst");
    ValidateNotSameResource(base, dst, kFunction);
    ValidateNotSameResource(overlay, dst, kFunction);
    ValidateNotSameResource(mask, dst, kFunction);
    ValidateHasSrv(base, kFunction, "base");
    ValidateHasSrv(overlay, kFunction, "overlay");
    ValidateHasSrv(mask, kFunction, "mask");
    ValidateHasUav(dst, kFunction, "dst");
    ValidateChannel(desc.channel, kFunction);
    ValidateFinite(desc.opacity, kFunction, "opacity");

    const DXGI_FORMAT baseFormat = ResolveFormat(desc.baseFormat, base);
    const DXGI_FORMAT overlayFormat = ResolveFormat(desc.overlayFormat, overlay);
    const DXGI_FORMAT maskFormat = ResolveFormat(desc.maskFormat, mask);
    const DXGI_FORMAT dstFormat = ResolveFormat(desc.dstFormat, dst);
    ValidateRgbaLike(baseFormat, kFunction, "baseFormat");
    ValidateRgbaLike(overlayFormat, kFunction, "overlayFormat");
    ValidateRgbaLike(maskFormat, kFunction, "maskFormat");
    ValidateRgbaLike(dstFormat, kFunction, "dstFormat");
    ValidateRgbaUavSupport(*m_context, dstFormat, kFunction);

    const auto bd = base.GetTexture2DDesc();
    const auto od = overlay.GetTexture2DDesc();
    const auto md = mask.GetTexture2DDesc();
    const auto dd = dst.GetTexture2DDesc();
    const ProcessingRect br = ResolveRect(desc.baseRect, bd.Width, bd.Height);
    const ProcessingRect orr = ResolveRect(desc.overlayRect, od.Width, od.Height);
    const ProcessingRect mr = ResolveRect(desc.maskRect, md.Width, md.Height);
    const ProcessingRect dr = ResolveRect(desc.dstRect, dd.Width, dd.Height);
    ValidateRectInside(br, bd.Width, bd.Height, kFunction, "baseRect");
    ValidateRectInside(orr, od.Width, od.Height, kFunction, "overlayRect");
    ValidateRectInside(mr, md.Width, md.Height, kFunction, "maskRect");
    ValidateRectInside(dr, dd.Width, dd.Height, kFunction, "dstRect");
    ValidateSameSize(br, dr, kFunction, "baseRect", "dstRect");
    ValidateSameSize(orr, dr, kFunction, "overlayRect", "dstRect");
    ValidateSameSize(mr, dr, kFunction, "maskRect", "dstRect");

    auto bv = CreateRgbaTextureViewSet(*m_context, base, true, false, baseFormat);
    auto ov = CreateRgbaTextureViewSet(*m_context, overlay, true, false, overlayFormat);
    auto mv = CreateRgbaTextureViewSet(*m_context, mask, true, false, maskFormat);
    auto dv = CreateRgbaTextureViewSet(*m_context, dst, false, true, dstFormat);

    D3D11MaskConstants c = MakeCommonConstants(dr.width, dr.height, br, mr, dr);
    c.overlayX = orr.x;
    c.overlayY = orr.y;
    c.channel = static_cast<UINT>(desc.channel);
    c.invert = desc.invert ? 1u : 0u;
    c.opacity = desc.opacity;

    ID3D11ShaderResourceView* srvs[] = { bv.srv.Get(), ov.srv.Get(), mv.srv.Get() };
    BindAndDispatch(*m_context, m_pipelines->blend, deviceContext, srvs, 3, dv.uav.Get(), c, kFunction);
}

void D3D11MaskProcessor::DispatchCombineMasks(
    ID3D11DeviceContext* deviceContext,
    D3D11Resource& maskA,
    D3D11Resource& maskB,
    D3D11Resource& dst,
    const MaskCombineDesc& desc) {

    EnsurePipelines();
    constexpr const char* kFunction = "D3D11MaskProcessor::DispatchCombineMasks";

    ValidateTexture2D(maskA, kFunction, "maskA");
    ValidateTexture2D(maskB, kFunction, "maskB");
    ValidateTexture2D(dst, kFunction, "dst");
    ValidateNotSameResource(maskA, dst, kFunction);
    ValidateNotSameResource(maskB, dst, kFunction);
    ValidateHasSrv(maskA, kFunction, "maskA");
    ValidateHasSrv(maskB, kFunction, "maskB");
    ValidateHasUav(dst, kFunction, "dst");
    ValidateChannel(desc.channelA, kFunction);
    ValidateChannel(desc.channelB, kFunction);
    ValidateFinite(desc.scale, kFunction, "scale");
    ValidateFinite(desc.bias, kFunction, "bias");

    switch (desc.mode) {
    case MaskCombineMode::Add:
    case MaskCombineMode::Multiply:
    case MaskCombineMode::Max:
    case MaskCombineMode::Min:
    case MaskCombineMode::Subtract:
        break;
    default:
        throw ValidationError("D3D11MaskProcessor::DispatchCombineMasks: unsupported combine mode");
    }

    const DXGI_FORMAT maskAFormat = ResolveFormat(desc.maskAFormat, maskA);
    const DXGI_FORMAT maskBFormat = ResolveFormat(desc.maskBFormat, maskB);
    const DXGI_FORMAT dstFormat = ResolveFormat(desc.dstFormat, dst);
    ValidateRgbaLike(maskAFormat, kFunction, "maskAFormat");
    ValidateRgbaLike(maskBFormat, kFunction, "maskBFormat");
    ValidateRgbaLike(dstFormat, kFunction, "dstFormat");
    ValidateRgbaUavSupport(*m_context, dstFormat, kFunction);

    const auto ad = maskA.GetTexture2DDesc();
    const auto bd = maskB.GetTexture2DDesc();
    const auto dd = dst.GetTexture2DDesc();
    const ProcessingRect ar = ResolveRect(desc.maskARect, ad.Width, ad.Height);
    const ProcessingRect br = ResolveRect(desc.maskBRect, bd.Width, bd.Height);
    const ProcessingRect dr = ResolveRect(desc.dstRect, dd.Width, dd.Height);
    ValidateRectInside(ar, ad.Width, ad.Height, kFunction, "maskARect");
    ValidateRectInside(br, bd.Width, bd.Height, kFunction, "maskBRect");
    ValidateRectInside(dr, dd.Width, dd.Height, kFunction, "dstRect");
    ValidateSameSize(ar, dr, kFunction, "maskARect", "dstRect");
    ValidateSameSize(br, dr, kFunction, "maskBRect", "dstRect");

    auto av = CreateRgbaTextureViewSet(*m_context, maskA, true, false, maskAFormat);
    auto bv = CreateRgbaTextureViewSet(*m_context, maskB, true, false, maskBFormat);
    auto dv = CreateRgbaTextureViewSet(*m_context, dst, false, true, dstFormat);

    D3D11MaskConstants c = MakeCommonConstants(dr.width, dr.height, ar, br, dr);
    c.mode = static_cast<UINT>(desc.mode);
    c.channel = static_cast<UINT>(desc.channelA);
    c.channelB = static_cast<UINT>(desc.channelB);
    c.invert = desc.invertA ? 1u : 0u;
    c.invertB = desc.invertB ? 1u : 0u;
    c.scale = desc.scale;
    c.bias = desc.bias;

    ID3D11ShaderResourceView* srvs[] = { av.srv.Get(), bv.srv.Get() };
    BindAndDispatch(*m_context, m_pipelines->combine, deviceContext, srvs, 2, dv.uav.Get(), c, kFunction);
}

void D3D11MaskProcessor::DispatchInvertMask(
    ID3D11DeviceContext* deviceContext,
    D3D11Resource& mask,
    D3D11Resource& dst,
    const MaskInvertDesc& desc) {

    EnsurePipelines();
    constexpr const char* kFunction = "D3D11MaskProcessor::DispatchInvertMask";

    ValidateTexture2D(mask, kFunction, "mask");
    ValidateTexture2D(dst, kFunction, "dst");
    ValidateNotSameResource(mask, dst, kFunction);
    ValidateHasSrv(mask, kFunction, "mask");
    ValidateHasUav(dst, kFunction, "dst");
    ValidateChannel(desc.channel, kFunction);

    const DXGI_FORMAT maskFormat = ResolveFormat(desc.maskFormat, mask);
    const DXGI_FORMAT dstFormat = ResolveFormat(desc.dstFormat, dst);
    ValidateRgbaLike(maskFormat, kFunction, "maskFormat");
    ValidateRgbaLike(dstFormat, kFunction, "dstFormat");
    ValidateRgbaUavSupport(*m_context, dstFormat, kFunction);

    const auto md = mask.GetTexture2DDesc();
    const auto dd = dst.GetTexture2DDesc();
    const ProcessingRect mr = ResolveRect(desc.maskRect, md.Width, md.Height);
    const ProcessingRect dr = ResolveRect(desc.dstRect, dd.Width, dd.Height);
    ValidateRectInside(mr, md.Width, md.Height, kFunction, "maskRect");
    ValidateRectInside(dr, dd.Width, dd.Height, kFunction, "dstRect");
    ValidateSameSize(mr, dr, kFunction, "maskRect", "dstRect");

    auto mv = CreateRgbaTextureViewSet(*m_context, mask, true, false, maskFormat);
    auto dv = CreateRgbaTextureViewSet(*m_context, dst, false, true, dstFormat);

    D3D11MaskConstants c = MakeCommonConstants(dr.width, dr.height, mr, mr, dr);
    c.channel = static_cast<UINT>(desc.channel);

    ID3D11ShaderResourceView* srvs[] = { mv.srv.Get() };
    BindAndDispatch(*m_context, m_pipelines->invert, deviceContext, srvs, 1, dv.uav.Get(), c, kFunction);
}

D3D11Resource D3D11MaskProcessor::CreateOutputTexture(D3D11Core& core, UINT width, UINT height, DXGI_FORMAT format) {
    EnsureInitialized();
    if (width == 0 || height == 0) {
        throw ValidationError("D3D11MaskProcessor::CreateOutputTexture: size is zero");
    }
    if (!IsRgbaLikeFormat(format)) {
        throw UnsupportedFormatError("D3D11MaskProcessor::CreateOutputTexture: only RGBA-like formats are supported");
    }
    ValidateRgbaUavSupport(*m_context, format, "D3D11MaskProcessor::CreateOutputTexture");
    return CreateTexture2D(core, width, height, format, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
}

} // namespace Processing
} // namespace D3D11CoreLib
