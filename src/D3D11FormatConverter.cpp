#include "D3D11Processing/D3D11FormatConverter.hpp"
#include "D3D11Core/ThrowIfFailed.hpp"
#include "D3D11Framework/D3D11Helpers.hpp"

#include <algorithm>
#include <sstream>

namespace D3D11CoreLib {
namespace Processing {
namespace {

constexpr UINT kThreadGroupX = 16;
constexpr UINT kThreadGroupY = 16;

struct D3D11ProcessingConstants {
    UINT srcWidth = 0;
    UINT srcHeight = 0;
    UINT dstWidth = 0;
    UINT dstHeight = 0;
    INT srcX = 0;
    INT srcY = 0;
    INT dstX = 0;
    INT dstY = 0;
    UINT srcFormat = 0;
    UINT dstFormat = 0;
    UINT srcMatrix = 0;
    UINT srcRange = 0;
    UINT dstMatrix = 0;
    UINT dstRange = 0;
    UINT filter = 0;
    UINT alphaMode = 0;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    UINT mapFormat = 0;
    UINT coordinateMode = 0;
    UINT borderMode = 0;
    UINT blendMode = 0;
    float opacity = 1.0f;
    UINT reserved0 = 0;
    float borderColor[4] = { 0, 0, 0, 0 };
    UINT baseWidth = 0;
    UINT baseHeight = 0;
    UINT overlayWidth = 0;
    UINT overlayHeight = 0;
    INT baseX = 0;
    INT baseY = 0;
    INT overlayX = 0;
    INT overlayY = 0;
};
static_assert((sizeof(D3D11ProcessingConstants) % 16) == 0, "constant buffer size must be 16-byte aligned");

UINT DivideRoundUp(UINT value, UINT divisor) noexcept {
    return (value + divisor - 1u) / divisor;
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

void ValidateOutputUav(const D3D11Resource& resource, const char* functionName) {
    const auto desc = resource.GetTexture2DDesc();
    if ((desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) == 0) {
        std::ostringstream os;
        os << functionName << ": destination texture must have D3D11_BIND_UNORDERED_ACCESS";
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

DXGI_FORMAT ResolveFormat(DXGI_FORMAT requested, const D3D11Resource& resource) {
    return requested == DXGI_FORMAT_UNKNOWN ? resource.GetFormat() : requested;
}

void BindAndDispatch(
    D3D11ProcessingContext& context,
    const D3D11ComputePipeline& pipeline,
    ID3D11DeviceContext* deviceContext,
    ID3D11ShaderResourceView* const* srvs,
    UINT srvCount,
    ID3D11UnorderedAccessView* const* uavs,
    UINT uavCount,
    const D3D11ProcessingConstants& constants,
    UINT dispatchWidth,
    UINT dispatchHeight) {

    if (!deviceContext) {
        throw ValidationError("D3D11Processing: null device context");
    }

    context.UpdateConstants(deviceContext, &constants, static_cast<UINT>(sizeof(constants)));
    ID3D11Buffer* cb = context.ConstantBuffer();
    deviceContext->CSSetConstantBuffers(0, 1, &cb);
    if (srvCount > 0) deviceContext->CSSetShaderResources(0, srvCount, srvs);
    if (uavCount > 0) {
        UINT initialCounts[D3D11_PS_CS_UAV_REGISTER_COUNT] = {};
        std::fill_n(initialCounts, D3D11_PS_CS_UAV_REGISTER_COUNT, static_cast<UINT>(-1));
        deviceContext->CSSetUnorderedAccessViews(0, uavCount, uavs, initialCounts);
    }

    pipeline.Bind(deviceContext);
    deviceContext->Dispatch(DivideRoundUp(dispatchWidth, kThreadGroupX), DivideRoundUp(dispatchHeight, kThreadGroupY), 1);

    ID3D11ShaderResourceView* nullSrvs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
    ID3D11UnorderedAccessView* nullUavs[D3D11_PS_CS_UAV_REGISTER_COUNT] = {};
    ID3D11Buffer* nullCb = nullptr;
    if (srvCount > 0) deviceContext->CSSetShaderResources(0, srvCount, nullSrvs);
    if (uavCount > 0) deviceContext->CSSetUnorderedAccessViews(0, uavCount, nullUavs, nullptr);
    deviceContext->CSSetConstantBuffers(0, 1, &nullCb);
    pipeline.Unbind(deviceContext);
}

D3D11ProcessingConstants MakeConvertConstants(const D3D11Resource& src, const D3D11Resource& dst, const FormatConvertDesc& desc) {
    const auto srcDesc = src.GetTexture2DDesc();
    const auto dstDesc = dst.GetTexture2DDesc();
    const ProcessingRect srcRect = ResolveRect(desc.srcRect, srcDesc.Width, srcDesc.Height);
    const ProcessingRect dstRect = ResolveRect(desc.dstRect, dstDesc.Width, dstDesc.Height);
    ValidateRectInside(srcRect, srcDesc.Width, srcDesc.Height, "D3D11FormatConverter::DispatchConvert", "srcRect");
    ValidateRectInside(dstRect, dstDesc.Width, dstDesc.Height, "D3D11FormatConverter::DispatchConvert", "dstRect");
    if (srcRect.width != dstRect.width || srcRect.height != dstRect.height) {
        throw ValidationError("D3D11FormatConverter::DispatchConvert: format conversion does not resize; use D3D11Resizer or D3D11FusedProcessor");
    }
    D3D11ProcessingConstants c = {};
    c.srcWidth = srcRect.width;
    c.srcHeight = srcRect.height;
    c.dstWidth = dstRect.width;
    c.dstHeight = dstRect.height;
    c.srcX = srcRect.x;
    c.srcY = srcRect.y;
    c.dstX = dstRect.x;
    c.dstY = dstRect.y;
    c.srcFormat = static_cast<UINT>(ResolveFormat(desc.srcFormat, src));
    c.dstFormat = static_cast<UINT>(ResolveFormat(desc.dstFormat, dst));
    c.srcMatrix = static_cast<UINT>(desc.color.srcMatrix);
    c.srcRange = static_cast<UINT>(desc.color.srcRange);
    c.dstMatrix = static_cast<UINT>(desc.color.dstMatrix);
    c.dstRange = static_cast<UINT>(desc.color.dstRange);
    c.alphaMode = static_cast<UINT>(desc.color.alphaMode);
    return c;
}

D3D11ProcessingConstants MakeResizeConstants(const D3D11Resource& src, const D3D11Resource& dst, const ResizeDesc& desc) {
    const auto srcDesc = src.GetTexture2DDesc();
    const auto dstDesc = dst.GetTexture2DDesc();
    const ProcessingRect srcRect = ResolveRect(desc.srcRect, srcDesc.Width, srcDesc.Height);
    const ProcessingRect dstRect = ResolveRect(desc.dstRect, dstDesc.Width, dstDesc.Height);
    ValidateRectInside(srcRect, srcDesc.Width, srcDesc.Height, "D3D11Resizer::DispatchResize", "srcRect");
    ValidateRectInside(dstRect, dstDesc.Width, dstDesc.Height, "D3D11Resizer::DispatchResize", "dstRect");
    D3D11ProcessingConstants c = {};
    c.srcWidth = srcRect.width;
    c.srcHeight = srcRect.height;
    c.dstWidth = dstRect.width;
    c.dstHeight = dstRect.height;
    c.srcX = srcRect.x;
    c.srcY = srcRect.y;
    c.dstX = dstRect.x;
    c.dstY = dstRect.y;
    c.srcFormat = static_cast<UINT>(src.GetFormat());
    c.dstFormat = static_cast<UINT>(dst.GetFormat());
    c.filter = static_cast<UINT>(desc.filter);
    c.scaleX = static_cast<float>(srcRect.width) / static_cast<float>(dstRect.width);
    c.scaleY = static_cast<float>(srcRect.height) / static_cast<float>(dstRect.height);
    return c;
}

D3D11ProcessingConstants MakeFusedConstants(const D3D11Resource& src, const D3D11Resource& dst, const FusedConvertResizeDesc& desc) {
    const auto srcDesc = src.GetTexture2DDesc();
    const auto dstDesc = dst.GetTexture2DDesc();
    const ProcessingRect srcRect = ResolveRect(desc.srcRect, srcDesc.Width, srcDesc.Height);
    const ProcessingRect dstRect = ResolveRect(desc.dstRect, dstDesc.Width, dstDesc.Height);
    ValidateRectInside(srcRect, srcDesc.Width, srcDesc.Height, "D3D11FusedProcessor::DispatchConvertResize", "srcRect");
    ValidateRectInside(dstRect, dstDesc.Width, dstDesc.Height, "D3D11FusedProcessor::DispatchConvertResize", "dstRect");
    D3D11ProcessingConstants c = {};
    c.srcWidth = srcRect.width;
    c.srcHeight = srcRect.height;
    c.dstWidth = dstRect.width;
    c.dstHeight = dstRect.height;
    c.srcX = srcRect.x;
    c.srcY = srcRect.y;
    c.dstX = dstRect.x;
    c.dstY = dstRect.y;
    c.srcFormat = static_cast<UINT>(ResolveFormat(desc.srcFormat, src));
    c.dstFormat = static_cast<UINT>(ResolveFormat(desc.dstFormat, dst));
    c.srcMatrix = static_cast<UINT>(desc.color.srcMatrix);
    c.srcRange = static_cast<UINT>(desc.color.srcRange);
    c.dstMatrix = static_cast<UINT>(desc.color.dstMatrix);
    c.dstRange = static_cast<UINT>(desc.color.dstRange);
    c.alphaMode = static_cast<UINT>(desc.color.alphaMode);
    c.filter = static_cast<UINT>(desc.filter);
    c.scaleX = static_cast<float>(srcRect.width) / static_cast<float>(dstRect.width);
    c.scaleY = static_cast<float>(srcRect.height) / static_cast<float>(dstRect.height);
    return c;
}

} // namespace

struct D3D11FormatConverter::Pipelines {
    D3D11ComputePipeline rgbToRgb;
    D3D11ComputePipeline yuv420ToRgb;
    D3D11ComputePipeline rgbToYuv420;
    bool initialized = false;
};

D3D11FormatConverter::D3D11FormatConverter() = default;
D3D11FormatConverter::~D3D11FormatConverter() = default;
D3D11FormatConverter::D3D11FormatConverter(D3D11FormatConverter&&) noexcept = default;
D3D11FormatConverter& D3D11FormatConverter::operator=(D3D11FormatConverter&&) noexcept = default;

void D3D11FormatConverter::Initialize(D3D11ProcessingContext& context) {
    m_context = &context;
    m_shaderCache.Initialize(context);
    m_pipelines.reset();
}

void D3D11FormatConverter::EnsureInitialized() const {
    if (!m_context) throw ValidationError("D3D11FormatConverter: converter is not initialized");
}

void D3D11FormatConverter::EnsurePipelines() {
    EnsureInitialized();
    if (!m_pipelines) m_pipelines.reset(new Pipelines());
    if (m_pipelines->initialized) return;
    auto* device = m_context->GetDevice();
    m_pipelines->rgbToRgb.Initialize(device, m_shaderCache.GetComputeShader("ConvertRgbToRgb.hlsl"));
    m_pipelines->yuv420ToRgb.Initialize(device, m_shaderCache.GetComputeShader("ConvertYuv420ToRgb.hlsl"));
    m_pipelines->rgbToYuv420.Initialize(device, m_shaderCache.GetComputeShader("ConvertRgbToYuv420.hlsl"));
    m_pipelines->initialized = true;
}

void D3D11FormatConverter::DispatchConvert(ID3D11DeviceContext* deviceContext, D3D11Resource& src, D3D11Resource& dst, const FormatConvertDesc& desc) {
    EnsurePipelines();
    ValidateTexture2D(src, "D3D11FormatConverter::DispatchConvert", "src");
    ValidateTexture2D(dst, "D3D11FormatConverter::DispatchConvert", "dst");
    ValidateNotSameResource(src, dst, "D3D11FormatConverter::DispatchConvert");
    ValidateOutputUav(dst, "D3D11FormatConverter::DispatchConvert");

    auto constants = MakeConvertConstants(src, dst, desc);
    const DXGI_FORMAT srcFormat = static_cast<DXGI_FORMAT>(constants.srcFormat);
    const DXGI_FORMAT dstFormat = static_cast<DXGI_FORMAT>(constants.dstFormat);
    if (!IsSupportedProcessingFormat(srcFormat) || !IsSupportedProcessingFormat(dstFormat)) {
        throw UnsupportedFormatError("D3D11FormatConverter::DispatchConvert: unsupported format");
    }
    if (IsYuv420Format(srcFormat)) ValidateYuv420Rect(ResolveRect(desc.srcRect, src.GetWidth(), src.GetHeight()), "D3D11FormatConverter::DispatchConvert", "srcRect");
    if (IsYuv420Format(dstFormat)) ValidateYuv420Rect(ResolveRect(desc.dstRect, dst.GetWidth(), dst.GetHeight()), "D3D11FormatConverter::DispatchConvert", "dstRect");

    if (IsRgbaLikeFormat(srcFormat) && IsRgbaLikeFormat(dstFormat)) {
        auto sv = CreateRgbaTextureViewSet(*m_context, src, true, false, srcFormat);
        auto dv = CreateRgbaTextureViewSet(*m_context, dst, false, true, dstFormat);
        ID3D11ShaderResourceView* srvs[] = { sv.srv.Get() };
        ID3D11UnorderedAccessView* uavs[] = { dv.uav.Get() };
        BindAndDispatch(*m_context, m_pipelines->rgbToRgb, deviceContext, srvs, 1, uavs, 1, constants, constants.dstWidth, constants.dstHeight);
    } else if (IsYuv420Format(srcFormat) && IsRgbaLikeFormat(dstFormat)) {
        auto sv = CreateYuv420SrvViewSet(*m_context, src);
        auto dv = CreateRgbaTextureViewSet(*m_context, dst, false, true, dstFormat);
        ID3D11ShaderResourceView* srvs[] = { sv.ySrv.Get(), sv.uvSrv.Get() };
        ID3D11UnorderedAccessView* uavs[] = { dv.uav.Get() };
        BindAndDispatch(*m_context, m_pipelines->yuv420ToRgb, deviceContext, srvs, 2, uavs, 1, constants, constants.dstWidth, constants.dstHeight);
    } else if (IsRgbaLikeFormat(srcFormat) && IsYuv420Format(dstFormat)) {
        auto sv = CreateRgbaTextureViewSet(*m_context, src, true, false, srcFormat);
        auto dv = CreateYuv420UavViewSet(*m_context, dst);
        ID3D11ShaderResourceView* srvs[] = { sv.srv.Get() };
        ID3D11UnorderedAccessView* uavs[] = { dv.yUav.Get(), dv.uvUav.Get() };
        BindAndDispatch(*m_context, m_pipelines->rgbToYuv420, deviceContext, srvs, 1, uavs, 2, constants, constants.dstWidth, constants.dstHeight);
    } else {
        throw UnsupportedFormatError("D3D11FormatConverter::DispatchConvert: unsupported conversion matrix");
    }
}

D3D11Resource D3D11FormatConverter::CreateOutputTexture(D3D11Core& core, UINT width, UINT height, DXGI_FORMAT format) {
    EnsureInitialized();
    if (width == 0 || height == 0) throw ValidationError("D3D11FormatConverter::CreateOutputTexture: size is zero");
    if (!IsSupportedProcessingFormat(format)) throw UnsupportedFormatError("D3D11FormatConverter::CreateOutputTexture: unsupported output format");
    ValidateEvenSize(width, height, format, "D3D11FormatConverter::CreateOutputTexture");
    if (format == DXGI_FORMAT_R8G8B8A8_UNORM && !m_context->SupportsRgba8Uav()) throw UnsupportedFeatureError("R8G8B8A8 UAV is not supported");
    if (format == DXGI_FORMAT_B8G8R8A8_UNORM && !m_context->SupportsBgra8Uav()) throw UnsupportedFeatureError("B8G8R8A8 UAV is not supported");
    if (format == DXGI_FORMAT_R16G16B16A16_FLOAT && !m_context->SupportsRgba16FloatUav()) throw UnsupportedFeatureError("RGBA16F UAV is not supported");
    if (format == DXGI_FORMAT_NV12 && !m_context->SupportsNv12Uav()) throw UnsupportedFeatureError("NV12 UAV is not supported");
    if (format == DXGI_FORMAT_P010 && !m_context->SupportsP010Uav()) throw UnsupportedFeatureError("P010 UAV is not supported");
    return CreateTexture2D(core, width, height, format, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
}

} // namespace Processing
} // namespace D3D11CoreLib
