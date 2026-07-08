#include <D3D11Helper/D3D11Processing/D3D11AdvancedProcessing.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Helpers.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace D3D11CoreLib {
namespace Processing {
namespace {

constexpr UINT kThreadGroupX = 16;
constexpr UINT kThreadGroupY = 16;

struct D3D11AdvancedTransformConstants {
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
    UINT filter = 0;
    UINT borderMode = 0;

    float matrixRow0[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
    float matrixRow1[4] = { 0.0f, 1.0f, 0.0f, 0.0f };
    float matrixRow2[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
    float borderColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};
static_assert((sizeof(D3D11AdvancedTransformConstants) % 16) == 0, "constant buffer size must be 16-byte aligned");

struct D3D11Lut3DConstants {
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
    UINT lutWidth = 0;
    UINT lutHeight = 0;

    UINT lutDepth = 0;
    UINT preserveAlpha = 1;
    float strength = 1.0f;
    UINT reserved0 = 0;
};
static_assert((sizeof(D3D11Lut3DConstants) % 16) == 0, "constant buffer size must be 16-byte aligned");

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

D3D11_TEXTURE3D_DESC GetTexture3DDesc(const D3D11Resource& resource, const char* functionName, const char* argumentName) {
    if (!resource.Get()) {
        std::ostringstream os;
        os << functionName << ": " << argumentName << " is null";
        throw ValidationError(os.str());
    }
    ComPtr<ID3D11Texture3D> tex3d;
    if (FAILED(resource.Get()->QueryInterface(IID_PPV_ARGS(&tex3d)))) {
        std::ostringstream os;
        os << functionName << ": " << argumentName << " is not Texture3D";
        throw ValidationError(os.str());
    }
    D3D11_TEXTURE3D_DESC desc = {};
    tex3d->GetDesc(&desc);
    if (desc.Width == 0 || desc.Height == 0 || desc.Depth == 0) {
        std::ostringstream os;
        os << functionName << ": " << argumentName << " has zero size";
        throw ValidationError(os.str());
    }
    return desc;
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

void ValidateFilterAndBorder(ProcessingFilter filter, RemapBorderMode borderMode, const char* functionName) {
    if (filter != ProcessingFilter::Point && filter != ProcessingFilter::Linear) {
        std::ostringstream os;
        os << functionName << ": unsupported filter";
        throw ValidationError(os.str());
    }
    if (borderMode != RemapBorderMode::Clamp && borderMode != RemapBorderMode::Constant) {
        std::ostringstream os;
        os << functionName << ": unsupported border mode";
        throw ValidationError(os.str());
    }
}

void ValidateMatrixValues(const float* values, UINT count, const char* functionName) {
    for (UINT i = 0; i < count; ++i) {
        if (!std::isfinite(values[i])) {
            std::ostringstream os;
            os << functionName << ": matrix contains non-finite value";
            throw ValidationError(os.str());
        }
    }
}

void BindAndDispatchTransform(
    D3D11ProcessingContext& context,
    const D3D11ComputePipeline& pipeline,
    ID3D11DeviceContext* deviceContext,
    ID3D11ShaderResourceView* srv,
    ID3D11UnorderedAccessView* uav,
    const D3D11AdvancedTransformConstants& constants) {

    if (!deviceContext) {
        throw ValidationError("D3D11AdvancedProcessor: null device context");
    }

    context.UpdateConstants(deviceContext, &constants, static_cast<UINT>(sizeof(constants)));
    ID3D11Buffer* cb = context.ConstantBuffer();
    deviceContext->CSSetConstantBuffers(0, 1, &cb);
    deviceContext->CSSetShaderResources(0, 1, &srv);
    UINT initialCounts[D3D11_PS_CS_UAV_REGISTER_COUNT] = {};
    std::fill_n(initialCounts, D3D11_PS_CS_UAV_REGISTER_COUNT, static_cast<UINT>(-1));
    deviceContext->CSSetUnorderedAccessViews(0, 1, &uav, initialCounts);

    pipeline.Bind(deviceContext);
    deviceContext->Dispatch(DivideRoundUp(constants.dstWidth, kThreadGroupX), DivideRoundUp(constants.dstHeight, kThreadGroupY), 1);

    ID3D11ShaderResourceView* nullSrvs[1] = {};
    ID3D11UnorderedAccessView* nullUavs[1] = {};
    ID3D11Buffer* nullCb = nullptr;
    deviceContext->CSSetShaderResources(0, 1, nullSrvs);
    deviceContext->CSSetUnorderedAccessViews(0, 1, nullUavs, nullptr);
    deviceContext->CSSetConstantBuffers(0, 1, &nullCb);
    pipeline.Unbind(deviceContext);
}

void BindAndDispatchLut3D(
    D3D11ProcessingContext& context,
    const D3D11ComputePipeline& pipeline,
    ID3D11DeviceContext* deviceContext,
    ID3D11ShaderResourceView* const* srvs,
    ID3D11UnorderedAccessView* uav,
    const D3D11Lut3DConstants& constants) {

    if (!deviceContext) {
        throw ValidationError("D3D11AdvancedProcessor: null device context");
    }

    context.UpdateConstants(deviceContext, &constants, static_cast<UINT>(sizeof(constants)));
    ID3D11Buffer* cb = context.ConstantBuffer();
    deviceContext->CSSetConstantBuffers(0, 1, &cb);
    deviceContext->CSSetShaderResources(0, 2, srvs);
    UINT initialCounts[D3D11_PS_CS_UAV_REGISTER_COUNT] = {};
    std::fill_n(initialCounts, D3D11_PS_CS_UAV_REGISTER_COUNT, static_cast<UINT>(-1));
    deviceContext->CSSetUnorderedAccessViews(0, 1, &uav, initialCounts);

    pipeline.Bind(deviceContext);
    deviceContext->Dispatch(DivideRoundUp(constants.dstWidth, kThreadGroupX), DivideRoundUp(constants.dstHeight, kThreadGroupY), 1);

    ID3D11ShaderResourceView* nullSrvs[2] = {};
    ID3D11UnorderedAccessView* nullUavs[1] = {};
    ID3D11Buffer* nullCb = nullptr;
    deviceContext->CSSetShaderResources(0, 2, nullSrvs);
    deviceContext->CSSetUnorderedAccessViews(0, 1, nullUavs, nullptr);
    deviceContext->CSSetConstantBuffers(0, 1, &nullCb);
    pipeline.Unbind(deviceContext);
}

D3D11AdvancedTransformConstants MakeTransformConstants(
    const D3D11Resource& src,
    const D3D11Resource& dst,
    DXGI_FORMAT srcFormat,
    DXGI_FORMAT dstFormat,
    ProcessingFilter filter,
    RemapBorderMode borderMode,
    const float* matrix9,
    const float* borderColor,
    const ProcessingRect& srcRectIn,
    const ProcessingRect& dstRectIn,
    const char* functionName) {

    const auto srcDesc = src.GetTexture2DDesc();
    const auto dstDesc = dst.GetTexture2DDesc();
    const auto srcRect = ResolveRect(srcRectIn, srcDesc.Width, srcDesc.Height);
    const auto dstRect = ResolveRect(dstRectIn, dstDesc.Width, dstDesc.Height);
    ValidateRectInside(srcRect, srcDesc.Width, srcDesc.Height, functionName, "srcRect");
    ValidateRectInside(dstRect, dstDesc.Width, dstDesc.Height, functionName, "dstRect");

    D3D11AdvancedTransformConstants c = {};
    c.srcWidth = srcRect.width;
    c.srcHeight = srcRect.height;
    c.dstWidth = dstRect.width;
    c.dstHeight = dstRect.height;
    c.srcX = srcRect.x;
    c.srcY = srcRect.y;
    c.dstX = dstRect.x;
    c.dstY = dstRect.y;
    c.srcFormat = static_cast<UINT>(srcFormat);
    c.dstFormat = static_cast<UINT>(dstFormat);
    c.filter = static_cast<UINT>(filter);
    c.borderMode = static_cast<UINT>(borderMode);
    c.matrixRow0[0] = matrix9[0];
    c.matrixRow0[1] = matrix9[1];
    c.matrixRow0[2] = matrix9[2];
    c.matrixRow1[0] = matrix9[3];
    c.matrixRow1[1] = matrix9[4];
    c.matrixRow1[2] = matrix9[5];
    c.matrixRow2[0] = matrix9[6];
    c.matrixRow2[1] = matrix9[7];
    c.matrixRow2[2] = matrix9[8];
    std::copy(borderColor, borderColor + 4, c.borderColor);
    return c;
}

D3D11Lut3DConstants MakeLutConstants(
    const D3D11Resource& src,
    const D3D11Resource& lut,
    const D3D11Resource& dst,
    const Lut3DDesc& desc,
    const D3D11_TEXTURE3D_DESC& lutTextureDesc) {

    const auto srcDesc = src.GetTexture2DDesc();
    const auto dstDesc = dst.GetTexture2DDesc();
    const auto srcRect = ResolveRect(desc.srcRect, srcDesc.Width, srcDesc.Height);
    const auto dstRect = ResolveRect(desc.dstRect, dstDesc.Width, dstDesc.Height);
    ValidateRectInside(srcRect, srcDesc.Width, srcDesc.Height, "D3D11AdvancedProcessor::DispatchApplyLut3D", "srcRect");
    ValidateRectInside(dstRect, dstDesc.Width, dstDesc.Height, "D3D11AdvancedProcessor::DispatchApplyLut3D", "dstRect");

    D3D11Lut3DConstants c = {};
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
    c.lutWidth = desc.lutWidth ? desc.lutWidth : lutTextureDesc.Width;
    c.lutHeight = desc.lutHeight ? desc.lutHeight : lutTextureDesc.Height;
    c.lutDepth = desc.lutDepth ? desc.lutDepth : lutTextureDesc.Depth;
    c.preserveAlpha = desc.preserveAlpha ? 1u : 0u;
    c.strength = desc.strength;
    (void)lut;
    return c;
}

D3D11_SHADER_RESOURCE_VIEW_DESC MakeTexture2DSrvDesc(DXGI_FORMAT format) {
    D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
    desc.Format = format;
    desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    desc.Texture2D.MostDetailedMip = 0;
    desc.Texture2D.MipLevels = 1;
    return desc;
}

D3D11_SHADER_RESOURCE_VIEW_DESC MakeTexture3DSrvDesc(DXGI_FORMAT format) {
    D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
    desc.Format = format;
    desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
    desc.Texture3D.MostDetailedMip = 0;
    desc.Texture3D.MipLevels = 1;
    return desc;
}

D3D11_UNORDERED_ACCESS_VIEW_DESC MakeTexture2DUavDesc(DXGI_FORMAT format) {
    D3D11_UNORDERED_ACCESS_VIEW_DESC desc = {};
    desc.Format = format;
    desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    desc.Texture2D.MipSlice = 0;
    return desc;
}

} // namespace

struct D3D11AdvancedProcessor::Pipelines {
    D3D11ComputePipeline transform;
    D3D11ComputePipeline lut3d;
    bool initialized = false;
};

D3D11AdvancedProcessor::D3D11AdvancedProcessor() = default;
D3D11AdvancedProcessor::~D3D11AdvancedProcessor() = default;
D3D11AdvancedProcessor::D3D11AdvancedProcessor(D3D11AdvancedProcessor&&) noexcept = default;
D3D11AdvancedProcessor& D3D11AdvancedProcessor::operator=(D3D11AdvancedProcessor&&) noexcept = default;

void D3D11AdvancedProcessor::Initialize(D3D11ProcessingContext& context) {
    m_context = &context;
    m_shaderCache.Initialize(context);
    m_remapper.Initialize(context);
    m_pipelines.reset();
}

void D3D11AdvancedProcessor::EnsureInitialized() const {
    if (!m_context) {
        throw ValidationError("D3D11AdvancedProcessor: processor is not initialized");
    }
}

void D3D11AdvancedProcessor::EnsurePipelines() {
    EnsureInitialized();
    if (!m_pipelines) {
        m_pipelines.reset(new Pipelines());
    }
    if (m_pipelines->initialized) {
        return;
    }

    m_pipelines->transform.Initialize(m_context->GetDevice(), m_shaderCache.GetComputeShader("AdvancedTransformRgba.hlsl"));
    m_pipelines->lut3d.Initialize(m_context->GetDevice(), m_shaderCache.GetComputeShader("ApplyLut3D.hlsl"));
    m_pipelines->initialized = true;
}

void D3D11AdvancedProcessor::DispatchAffineTransform(
    ID3D11DeviceContext* deviceContext,
    D3D11Resource& src,
    D3D11Resource& dst,
    const AffineTransformDesc& desc) {

    float matrix9[9] = {
        desc.dstToSrc[0], desc.dstToSrc[1], desc.dstToSrc[2],
        desc.dstToSrc[3], desc.dstToSrc[4], desc.dstToSrc[5],
        0.0f, 0.0f, 1.0f,
    };

    PerspectiveTransformDesc perspective = {};
    perspective.srcFormat = desc.srcFormat;
    perspective.dstFormat = desc.dstFormat;
    perspective.filter = desc.filter;
    perspective.srcRect = desc.srcRect;
    perspective.dstRect = desc.dstRect;
    std::copy(matrix9, matrix9 + 9, perspective.dstToSrc);
    perspective.borderMode = desc.borderMode;
    std::copy(desc.borderColor, desc.borderColor + 4, perspective.borderColor);

    DispatchPerspectiveTransform(deviceContext, src, dst, perspective);
}

void D3D11AdvancedProcessor::DispatchPerspectiveTransform(
    ID3D11DeviceContext* deviceContext,
    D3D11Resource& src,
    D3D11Resource& dst,
    const PerspectiveTransformDesc& desc) {

    EnsurePipelines();
    ValidateTexture2D(src, "D3D11AdvancedProcessor::DispatchPerspectiveTransform", "src");
    ValidateTexture2D(dst, "D3D11AdvancedProcessor::DispatchPerspectiveTransform", "dst");
    ValidateNotSameResource(src, dst, "D3D11AdvancedProcessor::DispatchPerspectiveTransform");
    ValidateOutputUav(dst, "D3D11AdvancedProcessor::DispatchPerspectiveTransform");
    ValidateFilterAndBorder(desc.filter, desc.borderMode, "D3D11AdvancedProcessor::DispatchPerspectiveTransform");
    ValidateMatrixValues(desc.dstToSrc, 9, "D3D11AdvancedProcessor::DispatchPerspectiveTransform");

    const DXGI_FORMAT srcFormat = ResolveFormat(desc.srcFormat, src);
    const DXGI_FORMAT dstFormat = ResolveFormat(desc.dstFormat, dst);
    if (!IsRgbaLikeFormat(srcFormat) || !IsRgbaLikeFormat(dstFormat)) {
        throw UnsupportedFormatError("D3D11AdvancedProcessor::DispatchPerspectiveTransform: only RGBA-like formats are supported");
    }

    const auto constants = MakeTransformConstants(
        src,
        dst,
        srcFormat,
        dstFormat,
        desc.filter,
        desc.borderMode,
        desc.dstToSrc,
        desc.borderColor,
        desc.srcRect,
        desc.dstRect,
        "D3D11AdvancedProcessor::DispatchPerspectiveTransform");

    auto srcSrv = CreateSrv(m_context->Core(), src.Get(), MakeTexture2DSrvDesc(srcFormat));
    auto dstUav = CreateUav(m_context->Core(), dst.Get(), MakeTexture2DUavDesc(dstFormat));
    BindAndDispatchTransform(*m_context, m_pipelines->transform, deviceContext, srcSrv.Get(), dstUav.Get(), constants);
}

void D3D11AdvancedProcessor::DispatchApplyLut3D(
    ID3D11DeviceContext* deviceContext,
    D3D11Resource& src,
    D3D11Resource& lut,
    D3D11Resource& dst,
    const Lut3DDesc& desc) {

    EnsurePipelines();
    ValidateTexture2D(src, "D3D11AdvancedProcessor::DispatchApplyLut3D", "src");
    const auto lutDesc = GetTexture3DDesc(lut, "D3D11AdvancedProcessor::DispatchApplyLut3D", "lut");
    ValidateTexture2D(dst, "D3D11AdvancedProcessor::DispatchApplyLut3D", "dst");
    ValidateNotSameResource(src, dst, "D3D11AdvancedProcessor::DispatchApplyLut3D");
    ValidateNotSameResource(lut, dst, "D3D11AdvancedProcessor::DispatchApplyLut3D");
    ValidateOutputUav(dst, "D3D11AdvancedProcessor::DispatchApplyLut3D");
    ValidateOpacity(desc.strength, "D3D11AdvancedProcessor::DispatchApplyLut3D");

    const DXGI_FORMAT srcFormat = ResolveFormat(desc.srcFormat, src);
    const DXGI_FORMAT dstFormat = ResolveFormat(desc.dstFormat, dst);
    const DXGI_FORMAT lutFormat = desc.lutFormat == DXGI_FORMAT_UNKNOWN ? lutDesc.Format : desc.lutFormat;
    if (!IsRgbaLikeFormat(srcFormat) || !IsRgbaLikeFormat(dstFormat) || !IsRgbaLikeFormat(lutFormat)) {
        throw UnsupportedFormatError("D3D11AdvancedProcessor::DispatchApplyLut3D: only RGBA-like src/lut/dst formats are supported");
    }

    const auto constants = MakeLutConstants(src, lut, dst, desc, lutDesc);
    if (constants.lutWidth == 0 || constants.lutHeight == 0 || constants.lutDepth == 0) {
        throw ValidationError("D3D11AdvancedProcessor::DispatchApplyLut3D: LUT dimensions are zero");
    }

    auto srcSrv = CreateSrv(m_context->Core(), src.Get(), MakeTexture2DSrvDesc(srcFormat));
    auto lutSrv = CreateSrv(m_context->Core(), lut.Get(), MakeTexture3DSrvDesc(lutFormat));
    auto dstUav = CreateUav(m_context->Core(), dst.Get(), MakeTexture2DUavDesc(dstFormat));
    ID3D11ShaderResourceView* srvs[] = { srcSrv.Get(), lutSrv.Get() };
    BindAndDispatchLut3D(*m_context, m_pipelines->lut3d, deviceContext, srvs, dstUav.Get(), constants);
}

void D3D11AdvancedProcessor::DispatchApplyUndistortMap(
    ID3D11DeviceContext* deviceContext,
    D3D11Resource& src,
    D3D11Resource& map,
    D3D11Resource& dst,
    const RemapDesc& desc) {

    EnsureInitialized();
    m_remapper.DispatchRemap(deviceContext, src, map, dst, desc);
}

D3D11Resource D3D11AdvancedProcessor::CreateOutputTexture(
    D3D11Core& core,
    UINT width,
    UINT height,
    DXGI_FORMAT format) {

    EnsureInitialized();
    if (width == 0 || height == 0) {
        throw ValidationError("D3D11AdvancedProcessor::CreateOutputTexture: size is zero");
    }
    if (!IsRgbaLikeFormat(format)) {
        throw UnsupportedFormatError("D3D11AdvancedProcessor::CreateOutputTexture: only RGBA-like formats are supported");
    }
    if (format == DXGI_FORMAT_R8G8B8A8_UNORM && !m_context->SupportsRgba8Uav()) {
        throw UnsupportedFeatureError("D3D11AdvancedProcessor::CreateOutputTexture: R8G8B8A8 UAV is not supported");
    }
    if (format == DXGI_FORMAT_B8G8R8A8_UNORM && !m_context->SupportsBgra8Uav()) {
        throw UnsupportedFeatureError("D3D11AdvancedProcessor::CreateOutputTexture: B8G8R8A8 UAV is not supported");
    }
    if (format == DXGI_FORMAT_R16G16B16A16_FLOAT && !m_context->SupportsRgba16FloatUav()) {
        throw UnsupportedFeatureError("D3D11AdvancedProcessor::CreateOutputTexture: RGBA16F UAV is not supported");
    }
    return CreateTexture2D(core, width, height, format, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
}

} // namespace Processing
} // namespace D3D11CoreLib
