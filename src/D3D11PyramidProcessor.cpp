#include <D3D11Helper/D3D11Processing/D3D11PyramidProcessor.hpp>
#include <D3D11Helper/D3D11Framework/D3D11Helpers.hpp>

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

namespace D3D11CoreLib {
namespace Processing {
namespace {

constexpr UINT kThreadGroupX = 16;
constexpr UINT kThreadGroupY = 16;

struct D3D11PyramidConstants {
    UINT srcWidth = 0;
    UINT srcHeight = 0;
    UINT dstWidth = 0;
    UINT dstHeight = 0;

    INT srcX = 0;
    INT srcY = 0;
    INT dstX = 0;
    INT dstY = 0;

    UINT filter = 0;
    UINT edgeMode = 0;
    UINT reserved0 = 0;
    UINT reserved1 = 0;

    float borderColor[4] = { 0, 0, 0, 0 };
};
static_assert((sizeof(D3D11PyramidConstants) % 16) == 0, "constant buffer size must be 16-byte aligned");
static_assert(sizeof(D3D11PyramidConstants) <= 256, "D3D11ProcessingContext constant buffer is 256 bytes");

UINT DivideRoundUp(UINT value, UINT divisor) noexcept {
    return (value + divisor - 1u) / divisor;
}

UINT HalfRoundUp(UINT value) noexcept {
    return value == 0 ? 0 : ((value + 1u) / 2u);
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

void ValidateRgbaFormats(
    D3D11ProcessingContext& context,
    DXGI_FORMAT srcFormat,
    DXGI_FORMAT dstFormat,
    const char* functionName) {

    if (!IsRgbaLikeFormat(srcFormat) || !IsRgbaLikeFormat(dstFormat)) {
        throw UnsupportedFormatError(std::string(functionName) + ": only RGBA-like formats are supported");
    }

    ValidateRgbaUavSupport(context, dstFormat, functionName);
}

void ValidatePyramidEdgeMode(PyramidEdgeMode mode, const char* functionName) {
    switch (mode) {
    case PyramidEdgeMode::Clamp:
    case PyramidEdgeMode::Constant:
        return;
    default:
        throw ValidationError(std::string(functionName) + ": unsupported pyramid edge mode");
    }
}

void ValidateProcessingFilter(ProcessingFilter filter, const char* functionName) {
    switch (filter) {
    case ProcessingFilter::Point:
    case ProcessingFilter::Linear:
        return;
    default:
        throw ValidationError(std::string(functionName) + ": unsupported upsample filter");
    }
}

D3D11PyramidConstants MakeConstants(
    const ProcessingRect& srcRect,
    const ProcessingRect& dstRect,
    UINT filter,
    PyramidEdgeMode edgeMode,
    const float borderColor[4]) {

    D3D11PyramidConstants c = {};
    c.srcWidth = srcRect.width;
    c.srcHeight = srcRect.height;
    c.dstWidth = dstRect.width;
    c.dstHeight = dstRect.height;
    c.srcX = srcRect.x;
    c.srcY = srcRect.y;
    c.dstX = dstRect.x;
    c.dstY = dstRect.y;
    c.filter = filter;
    c.edgeMode = static_cast<UINT>(edgeMode);
    std::copy(borderColor, borderColor + 4, c.borderColor);
    return c;
}

void BindAndDispatch(
    D3D11ProcessingContext& context,
    const D3D11ComputePipeline& pipeline,
    ID3D11DeviceContext* deviceContext,
    ID3D11ShaderResourceView* srv,
    ID3D11UnorderedAccessView* uav,
    const D3D11PyramidConstants& constants,
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

struct D3D11PyramidProcessor::Pipelines {
    D3D11ComputePipeline downsample2x;
    D3D11ComputePipeline upsample2x;
    bool initialized = false;
};

D3D11PyramidProcessor::D3D11PyramidProcessor() = default;
D3D11PyramidProcessor::~D3D11PyramidProcessor() = default;
D3D11PyramidProcessor::D3D11PyramidProcessor(D3D11PyramidProcessor&&) noexcept = default;
D3D11PyramidProcessor& D3D11PyramidProcessor::operator=(D3D11PyramidProcessor&&) noexcept = default;

void D3D11PyramidProcessor::Initialize(D3D11ProcessingContext& context) {
    m_context = &context;
    m_shaderCache.Initialize(context);
    m_pipelines.reset();
}

void D3D11PyramidProcessor::EnsureInitialized() const {
    if (!m_context) {
        throw ValidationError("D3D11PyramidProcessor: processor is not initialized");
    }
}

void D3D11PyramidProcessor::EnsurePipelines() {
    EnsureInitialized();

    if (!m_pipelines) {
        m_pipelines.reset(new Pipelines());
    }

    if (m_pipelines->initialized) {
        return;
    }

    auto* device = m_context->GetDevice();
    m_pipelines->downsample2x.Initialize(device, m_shaderCache.GetComputeShader("PyramidDownsample2xRgba.hlsl"));
    m_pipelines->upsample2x.Initialize(device, m_shaderCache.GetComputeShader("PyramidUpsample2xRgba.hlsl"));
    m_pipelines->initialized = true;
}

void D3D11PyramidProcessor::DispatchDownsample2x(
    ID3D11DeviceContext* deviceContext,
    D3D11Resource& src,
    D3D11Resource& dst,
    const PyramidDownsampleDesc& desc) {

    EnsurePipelines();

    constexpr const char* kFunction = "D3D11PyramidProcessor::DispatchDownsample2x";

    ValidateTexture2D(src, kFunction, "src");
    ValidateTexture2D(dst, kFunction, "dst");
    ValidateNotSameResource(src, dst, kFunction);
    ValidateHasSrv(src, kFunction, "src");
    ValidateHasUav(dst, kFunction, "dst");
    ValidatePyramidEdgeMode(desc.edgeMode, kFunction);

    const DXGI_FORMAT srcFormat = ResolveFormat(desc.srcFormat, src);
    const DXGI_FORMAT dstFormat = ResolveFormat(desc.dstFormat, dst);
    ValidateRgbaFormats(*m_context, srcFormat, dstFormat, kFunction);

    const auto srcTexDesc = src.GetTexture2DDesc();
    const auto dstTexDesc = dst.GetTexture2DDesc();
    const ProcessingRect srcRect = ResolveRect(desc.srcRect, srcTexDesc.Width, srcTexDesc.Height);
    const ProcessingRect dstRect = ResolveRect(desc.dstRect, dstTexDesc.Width, dstTexDesc.Height);

    ValidateRectInside(srcRect, srcTexDesc.Width, srcTexDesc.Height, kFunction, "srcRect");
    ValidateRectInside(dstRect, dstTexDesc.Width, dstTexDesc.Height, kFunction, "dstRect");

    const UINT expectedWidth = HalfRoundUp(srcRect.width);
    const UINT expectedHeight = HalfRoundUp(srcRect.height);
    if (dstRect.width != expectedWidth || dstRect.height != expectedHeight) {
        std::ostringstream os;
        os << kFunction << ": dstRect size must be ceil(srcRect / 2); expected "
           << expectedWidth << "x" << expectedHeight << " actual " << dstRect.width << "x" << dstRect.height;
        throw ValidationError(os.str());
    }

    auto srcViews = CreateRgbaTextureViewSet(*m_context, src, true, false, srcFormat);
    auto dstViews = CreateRgbaTextureViewSet(*m_context, dst, false, true, dstFormat);

    const auto constants = MakeConstants(srcRect, dstRect, 0, desc.edgeMode, desc.borderColor);
    BindAndDispatch(*m_context, m_pipelines->downsample2x, deviceContext, srcViews.srv.Get(), dstViews.uav.Get(), constants, kFunction);
}

void D3D11PyramidProcessor::DispatchUpsample2x(
    ID3D11DeviceContext* deviceContext,
    D3D11Resource& src,
    D3D11Resource& dst,
    const PyramidUpsampleDesc& desc) {

    EnsurePipelines();

    constexpr const char* kFunction = "D3D11PyramidProcessor::DispatchUpsample2x";

    ValidateTexture2D(src, kFunction, "src");
    ValidateTexture2D(dst, kFunction, "dst");
    ValidateNotSameResource(src, dst, kFunction);
    ValidateHasSrv(src, kFunction, "src");
    ValidateHasUav(dst, kFunction, "dst");
    ValidateProcessingFilter(desc.filter, kFunction);
    ValidatePyramidEdgeMode(desc.edgeMode, kFunction);

    const DXGI_FORMAT srcFormat = ResolveFormat(desc.srcFormat, src);
    const DXGI_FORMAT dstFormat = ResolveFormat(desc.dstFormat, dst);
    ValidateRgbaFormats(*m_context, srcFormat, dstFormat, kFunction);

    const auto srcTexDesc = src.GetTexture2DDesc();
    const auto dstTexDesc = dst.GetTexture2DDesc();
    const ProcessingRect srcRect = ResolveRect(desc.srcRect, srcTexDesc.Width, srcTexDesc.Height);
    const ProcessingRect dstRect = ResolveRect(desc.dstRect, dstTexDesc.Width, dstTexDesc.Height);

    ValidateRectInside(srcRect, srcTexDesc.Width, srcTexDesc.Height, kFunction, "srcRect");
    ValidateRectInside(dstRect, dstTexDesc.Width, dstTexDesc.Height, kFunction, "dstRect");

    const UINT expectedWidth = srcRect.width * 2u;
    const UINT expectedHeight = srcRect.height * 2u;
    if (dstRect.width != expectedWidth || dstRect.height != expectedHeight) {
        std::ostringstream os;
        os << kFunction << ": dstRect size must be srcRect * 2; expected "
           << expectedWidth << "x" << expectedHeight << " actual " << dstRect.width << "x" << dstRect.height;
        throw ValidationError(os.str());
    }

    auto srcViews = CreateRgbaTextureViewSet(*m_context, src, true, false, srcFormat);
    auto dstViews = CreateRgbaTextureViewSet(*m_context, dst, false, true, dstFormat);

    const auto constants = MakeConstants(srcRect, dstRect, static_cast<UINT>(desc.filter), desc.edgeMode, desc.borderColor);
    BindAndDispatch(*m_context, m_pipelines->upsample2x, deviceContext, srcViews.srv.Get(), dstViews.uav.Get(), constants, kFunction);
}

D3D11Resource D3D11PyramidProcessor::CreateOutputTexture(
    D3D11Core& core,
    UINT width,
    UINT height,
    DXGI_FORMAT format) {

    EnsureInitialized();

    if (width == 0 || height == 0) {
        throw ValidationError("D3D11PyramidProcessor::CreateOutputTexture: size is zero");
    }

    if (!IsRgbaLikeFormat(format)) {
        throw UnsupportedFormatError("D3D11PyramidProcessor::CreateOutputTexture: only RGBA-like formats are supported");
    }

    ValidateRgbaUavSupport(*m_context, format, "D3D11PyramidProcessor::CreateOutputTexture");

    return CreateTexture2D(
        core,
        width,
        height,
        format,
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
}

D3D11Resource D3D11PyramidProcessor::CreateDownsampledTexture(
    D3D11Core& core,
    UINT srcWidth,
    UINT srcHeight,
    DXGI_FORMAT format) {

    return CreateOutputTexture(core, HalfRoundUp(srcWidth), HalfRoundUp(srcHeight), format);
}

D3D11Resource D3D11PyramidProcessor::CreateUpsampledTexture(
    D3D11Core& core,
    UINT srcWidth,
    UINT srcHeight,
    DXGI_FORMAT format) {

    if (srcWidth > (0xffffffffu / 2u) || srcHeight > (0xffffffffu / 2u)) {
        throw ValidationError("D3D11PyramidProcessor::CreateUpsampledTexture: size overflow");
    }

    return CreateOutputTexture(core, srcWidth * 2u, srcHeight * 2u, format);
}

} // namespace Processing
} // namespace D3D11CoreLib
