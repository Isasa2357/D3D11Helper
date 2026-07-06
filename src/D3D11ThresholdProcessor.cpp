#include <D3D11Helper/D3D11Processing/D3D11ThresholdProcessor.hpp>
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

struct D3D11ThresholdConstants {
    UINT width = 0;
    UINT height = 0;
    INT srcX = 0;
    INT srcY = 0;

    INT dstX = 0;
    INT dstY = 0;
    UINT channel = 0;
    UINT invert = 0;

    float threshold = 0.5f;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float opacity = 1.0f;

    UINT mode = 0;
    UINT classCount = 16;
    float classScale = 255.0f;
    UINT reserved0 = 0;

    float foregroundColor[4] = { 1, 1, 1, 1 };
    float backgroundColor[4] = { 0, 0, 0, 1 };
    float overlayColor[4] = { 1, 0, 0, 1 };
};
static_assert((sizeof(D3D11ThresholdConstants) % 16) == 0, "constant buffer size must be 16-byte aligned");
static_assert(sizeof(D3D11ThresholdConstants) <= 256, "D3D11ProcessingContext constant buffer is 256 bytes");

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

void ValidateColor4(const float* v, const char* functionName, const char* fieldName) {
    for (UINT i = 0; i < 4; ++i) {
        if (!IsFinite(v[i])) {
            std::ostringstream os;
            os << functionName << ": " << fieldName << " contains non-finite value at component " << i;
            throw ValidationError(os.str());
        }
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
        throw ValidationError(std::string(functionName) + ": unsupported channel");
    }
}

void ValidateHeatmapMode(HeatmapMode mode, const char* functionName) {
    switch (mode) {
    case HeatmapMode::Grayscale:
    case HeatmapMode::RedGreen:
    case HeatmapMode::BlueRed:
    case HeatmapMode::TurboApprox:
        return;
    default:
        throw ValidationError(std::string(functionName) + ": unsupported heatmap mode");
    }
}

void ValidateSameSize(const ProcessingRect& a, const ProcessingRect& b, const char* functionName, const char* aName, const char* bName) {
    if (a.width != b.width || a.height != b.height) {
        std::ostringstream os;
        os << functionName << ": " << aName << " and " << bName << " sizes must match";
        throw ValidationError(os.str());
    }
}

D3D11ThresholdConstants MakeConstants(UINT width, UINT height, const ProcessingRect& srcRect, const ProcessingRect& dstRect) {
    D3D11ThresholdConstants c = {};
    c.width = width;
    c.height = height;
    c.srcX = srcRect.x;
    c.srcY = srcRect.y;
    c.dstX = dstRect.x;
    c.dstY = dstRect.y;
    return c;
}

void BindAndDispatch(
    D3D11ProcessingContext& context,
    const D3D11ComputePipeline& pipeline,
    ID3D11DeviceContext* deviceContext,
    ID3D11ShaderResourceView* srv,
    ID3D11UnorderedAccessView* uav,
    const D3D11ThresholdConstants& constants,
    const char* functionName) {

    if (!deviceContext) {
        throw ValidationError(std::string(functionName) + ": null device context");
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
    deviceContext->Dispatch(DivideRoundUp(constants.width, kThreadGroupX), DivideRoundUp(constants.height, kThreadGroupY), 1);

    ID3D11ShaderResourceView* nullSrvs[] = { nullptr };
    ID3D11UnorderedAccessView* nullUavs[] = { nullptr };
    ID3D11Buffer* nullCb = nullptr;
    deviceContext->CSSetShaderResources(0, 1, nullSrvs);
    deviceContext->CSSetUnorderedAccessViews(0, 1, nullUavs, nullptr);
    deviceContext->CSSetConstantBuffers(0, 1, &nullCb);
    pipeline.Unbind(deviceContext);
}

struct PreparedUnaryPass {
    DXGI_FORMAT srcFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT dstFormat = DXGI_FORMAT_UNKNOWN;
    ProcessingRect srcRect = {};
    ProcessingRect dstRect = {};
    D3D11TextureViewSet srcViews;
    D3D11TextureViewSet dstViews;
};

PreparedUnaryPass PrepareUnaryPass(
    D3D11ProcessingContext& context,
    D3D11Resource& src,
    D3D11Resource& dst,
    DXGI_FORMAT srcFormatRequest,
    DXGI_FORMAT dstFormatRequest,
    const ProcessingRect& srcRectRequest,
    const ProcessingRect& dstRectRequest,
    const char* functionName) {

    ValidateTexture2D(src, functionName, "src");
    ValidateTexture2D(dst, functionName, "dst");
    ValidateNotSameResource(src, dst, functionName);
    ValidateHasSrv(src, functionName, "src");
    ValidateHasUav(dst, functionName, "dst");

    PreparedUnaryPass pass;
    pass.srcFormat = ResolveFormat(srcFormatRequest, src);
    pass.dstFormat = ResolveFormat(dstFormatRequest, dst);
    ValidateRgbaLike(pass.srcFormat, functionName, "srcFormat");
    ValidateRgbaLike(pass.dstFormat, functionName, "dstFormat");
    ValidateRgbaUavSupport(context, pass.dstFormat, functionName);

    const auto srcDesc = src.GetTexture2DDesc();
    const auto dstDesc = dst.GetTexture2DDesc();
    pass.srcRect = ResolveRect(srcRectRequest, srcDesc.Width, srcDesc.Height);
    pass.dstRect = ResolveRect(dstRectRequest, dstDesc.Width, dstDesc.Height);
    ValidateRectInside(pass.srcRect, srcDesc.Width, srcDesc.Height, functionName, "srcRect");
    ValidateRectInside(pass.dstRect, dstDesc.Width, dstDesc.Height, functionName, "dstRect");
    ValidateSameSize(pass.srcRect, pass.dstRect, functionName, "srcRect", "dstRect");

    pass.srcViews = CreateRgbaTextureViewSet(context, src, true, false, pass.srcFormat);
    pass.dstViews = CreateRgbaTextureViewSet(context, dst, false, true, pass.dstFormat);
    return pass;
}

} // namespace

struct D3D11ThresholdProcessor::Pipelines {
    D3D11ComputePipeline threshold;
    D3D11ComputePipeline rangeThreshold;
    D3D11ComputePipeline heatmap;
    D3D11ComputePipeline classColor;
    D3D11ComputePipeline maskOverlay;
    bool initialized = false;
};

D3D11ThresholdProcessor::D3D11ThresholdProcessor() = default;
D3D11ThresholdProcessor::~D3D11ThresholdProcessor() = default;
D3D11ThresholdProcessor::D3D11ThresholdProcessor(D3D11ThresholdProcessor&&) noexcept = default;
D3D11ThresholdProcessor& D3D11ThresholdProcessor::operator=(D3D11ThresholdProcessor&&) noexcept = default;

void D3D11ThresholdProcessor::Initialize(D3D11ProcessingContext& context) {
    m_context = &context;
    m_shaderCache.Initialize(context);
    m_pipelines.reset();
}

void D3D11ThresholdProcessor::EnsureInitialized() const {
    if (!m_context) {
        throw ValidationError("D3D11ThresholdProcessor: processor is not initialized");
    }
}

void D3D11ThresholdProcessor::EnsurePipelines() {
    EnsureInitialized();
    if (!m_pipelines) {
        m_pipelines.reset(new Pipelines());
    }
    if (m_pipelines->initialized) {
        return;
    }

    auto* device = m_context->GetDevice();
    m_pipelines->threshold.Initialize(device, m_shaderCache.GetComputeShader("ThresholdRgba.hlsl"));
    m_pipelines->rangeThreshold.Initialize(device, m_shaderCache.GetComputeShader("RangeThresholdRgba.hlsl"));
    m_pipelines->heatmap.Initialize(device, m_shaderCache.GetComputeShader("ConfidenceHeatmapRgba.hlsl"));
    m_pipelines->classColor.Initialize(device, m_shaderCache.GetComputeShader("ClassColorMapRgba.hlsl"));
    m_pipelines->maskOverlay.Initialize(device, m_shaderCache.GetComputeShader("MaskOverlayRgba.hlsl"));
    m_pipelines->initialized = true;
}

void D3D11ThresholdProcessor::DispatchThreshold(
    ID3D11DeviceContext* deviceContext,
    D3D11Resource& src,
    D3D11Resource& dst,
    const ThresholdDesc& desc) {

    EnsurePipelines();
    constexpr const char* kFunction = "D3D11ThresholdProcessor::DispatchThreshold";
    ValidateChannel(desc.channel, kFunction);
    ValidateFinite(desc.threshold, kFunction, "threshold");
    ValidateColor4(desc.foregroundColor, kFunction, "foregroundColor");
    ValidateColor4(desc.backgroundColor, kFunction, "backgroundColor");

    auto pass = PrepareUnaryPass(*m_context, src, dst, desc.srcFormat, desc.dstFormat, desc.srcRect, desc.dstRect, kFunction);
    auto constants = MakeConstants(pass.srcRect.width, pass.srcRect.height, pass.srcRect, pass.dstRect);
    constants.channel = static_cast<UINT>(desc.channel);
    constants.invert = desc.invert ? 1u : 0u;
    constants.threshold = desc.threshold;
    std::copy(desc.foregroundColor, desc.foregroundColor + 4, constants.foregroundColor);
    std::copy(desc.backgroundColor, desc.backgroundColor + 4, constants.backgroundColor);

    BindAndDispatch(*m_context, m_pipelines->threshold, deviceContext, pass.srcViews.srv.Get(), pass.dstViews.uav.Get(), constants, kFunction);
}

void D3D11ThresholdProcessor::DispatchRangeThreshold(
    ID3D11DeviceContext* deviceContext,
    D3D11Resource& src,
    D3D11Resource& dst,
    const RangeThresholdDesc& desc) {

    EnsurePipelines();
    constexpr const char* kFunction = "D3D11ThresholdProcessor::DispatchRangeThreshold";
    ValidateChannel(desc.channel, kFunction);
    ValidateFinite(desc.minValue, kFunction, "minValue");
    ValidateFinite(desc.maxValue, kFunction, "maxValue");
    if (desc.minValue > desc.maxValue) {
        throw ValidationError("D3D11ThresholdProcessor::DispatchRangeThreshold: minValue must be <= maxValue");
    }
    ValidateColor4(desc.foregroundColor, kFunction, "foregroundColor");
    ValidateColor4(desc.backgroundColor, kFunction, "backgroundColor");

    auto pass = PrepareUnaryPass(*m_context, src, dst, desc.srcFormat, desc.dstFormat, desc.srcRect, desc.dstRect, kFunction);
    auto constants = MakeConstants(pass.srcRect.width, pass.srcRect.height, pass.srcRect, pass.dstRect);
    constants.channel = static_cast<UINT>(desc.channel);
    constants.invert = desc.invert ? 1u : 0u;
    constants.minValue = desc.minValue;
    constants.maxValue = desc.maxValue;
    std::copy(desc.foregroundColor, desc.foregroundColor + 4, constants.foregroundColor);
    std::copy(desc.backgroundColor, desc.backgroundColor + 4, constants.backgroundColor);

    BindAndDispatch(*m_context, m_pipelines->rangeThreshold, deviceContext, pass.srcViews.srv.Get(), pass.dstViews.uav.Get(), constants, kFunction);
}

void D3D11ThresholdProcessor::DispatchConfidenceHeatmap(
    ID3D11DeviceContext* deviceContext,
    D3D11Resource& src,
    D3D11Resource& dst,
    const ConfidenceHeatmapDesc& desc) {

    EnsurePipelines();
    constexpr const char* kFunction = "D3D11ThresholdProcessor::DispatchConfidenceHeatmap";
    ValidateChannel(desc.channel, kFunction);
    ValidateHeatmapMode(desc.mode, kFunction);
    ValidateFinite(desc.minValue, kFunction, "minValue");
    ValidateFinite(desc.maxValue, kFunction, "maxValue");
    ValidateFinite(desc.opacity, kFunction, "opacity");
    if (!(desc.maxValue > desc.minValue)) {
        throw ValidationError("D3D11ThresholdProcessor::DispatchConfidenceHeatmap: maxValue must be greater than minValue");
    }

    auto pass = PrepareUnaryPass(*m_context, src, dst, desc.srcFormat, desc.dstFormat, desc.srcRect, desc.dstRect, kFunction);
    auto constants = MakeConstants(pass.srcRect.width, pass.srcRect.height, pass.srcRect, pass.dstRect);
    constants.channel = static_cast<UINT>(desc.channel);
    constants.mode = static_cast<UINT>(desc.mode);
    constants.minValue = desc.minValue;
    constants.maxValue = desc.maxValue;
    constants.opacity = desc.opacity;

    BindAndDispatch(*m_context, m_pipelines->heatmap, deviceContext, pass.srcViews.srv.Get(), pass.dstViews.uav.Get(), constants, kFunction);
}

void D3D11ThresholdProcessor::DispatchClassColorMap(
    ID3D11DeviceContext* deviceContext,
    D3D11Resource& src,
    D3D11Resource& dst,
    const ClassColorMapDesc& desc) {

    EnsurePipelines();
    constexpr const char* kFunction = "D3D11ThresholdProcessor::DispatchClassColorMap";
    ValidateChannel(desc.channel, kFunction);
    ValidateFinite(desc.classScale, kFunction, "classScale");
    ValidateFinite(desc.opacity, kFunction, "opacity");
    if (desc.classScale < 0.0f) {
        throw ValidationError("D3D11ThresholdProcessor::DispatchClassColorMap: classScale must be non-negative");
    }
    if (desc.classCount == 0 || desc.classCount > 16) {
        throw ValidationError("D3D11ThresholdProcessor::DispatchClassColorMap: classCount must be in [1, 16]");
    }

    auto pass = PrepareUnaryPass(*m_context, src, dst, desc.srcFormat, desc.dstFormat, desc.srcRect, desc.dstRect, kFunction);
    auto constants = MakeConstants(pass.srcRect.width, pass.srcRect.height, pass.srcRect, pass.dstRect);
    constants.channel = static_cast<UINT>(desc.channel);
    constants.classScale = desc.classScale;
    constants.classCount = desc.classCount;
    constants.opacity = desc.opacity;

    BindAndDispatch(*m_context, m_pipelines->classColor, deviceContext, pass.srcViews.srv.Get(), pass.dstViews.uav.Get(), constants, kFunction);
}

void D3D11ThresholdProcessor::DispatchMaskOverlay(
    ID3D11DeviceContext* deviceContext,
    D3D11Resource& mask,
    D3D11Resource& dst,
    const MaskOverlayDesc& desc) {

    EnsurePipelines();
    constexpr const char* kFunction = "D3D11ThresholdProcessor::DispatchMaskOverlay";
    ValidateChannel(desc.channel, kFunction);
    ValidateFinite(desc.opacity, kFunction, "opacity");
    ValidateColor4(desc.overlayColor, kFunction, "overlayColor");

    auto pass = PrepareUnaryPass(*m_context, mask, dst, desc.maskFormat, desc.dstFormat, desc.maskRect, desc.dstRect, kFunction);
    auto constants = MakeConstants(pass.srcRect.width, pass.srcRect.height, pass.srcRect, pass.dstRect);
    constants.channel = static_cast<UINT>(desc.channel);
    constants.invert = desc.invert ? 1u : 0u;
    constants.opacity = desc.opacity;
    std::copy(desc.overlayColor, desc.overlayColor + 4, constants.overlayColor);

    BindAndDispatch(*m_context, m_pipelines->maskOverlay, deviceContext, pass.srcViews.srv.Get(), pass.dstViews.uav.Get(), constants, kFunction);
}

D3D11Resource D3D11ThresholdProcessor::CreateOutputTexture(
    D3D11Core& core,
    UINT width,
    UINT height,
    DXGI_FORMAT format) {

    EnsureInitialized();

    if (width == 0 || height == 0) {
        throw ValidationError("D3D11ThresholdProcessor::CreateOutputTexture: size is zero");
    }

    if (!IsRgbaLikeFormat(format)) {
        throw UnsupportedFormatError("D3D11ThresholdProcessor::CreateOutputTexture: only RGBA-like formats are supported");
    }

    ValidateRgbaUavSupport(*m_context, format, "D3D11ThresholdProcessor::CreateOutputTexture");

    return CreateTexture2D(
        core,
        width,
        height,
        format,
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
}

} // namespace Processing
} // namespace D3D11CoreLib
