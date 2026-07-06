#include <D3D11Helper/D3D11Processing/D3D11PyramidRegionBlur.hpp>
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

struct D3D11RegionBlurBlendConstants {
    UINT srcWidth = 0;
    UINT srcHeight = 0;
    UINT dstWidth = 0;
    UINT dstHeight = 0;

    INT srcX = 0;
    INT srcY = 0;
    INT dstX = 0;
    INT dstY = 0;

    UINT shape = 0;
    UINT selection = 0;
    UINT reserved0 = 0;
    UINT reserved1 = 0;

    float centerX = 0.0f;
    float centerY = 0.0f;
    float radius = 1.0f;
    float edgeSoftness = 0.0f;

    float rectX = 0.0f;
    float rectY = 0.0f;
    float rectWidth = 1.0f;
    float rectHeight = 1.0f;

    float blurStrength = 1.0f;
    float reserved2 = 0.0f;
    float reserved3 = 0.0f;
    float reserved4 = 0.0f;
};
static_assert((sizeof(D3D11RegionBlurBlendConstants) % 16) == 0, "constant buffer size must be 16-byte aligned");
static_assert(sizeof(D3D11RegionBlurBlendConstants) <= 256, "D3D11ProcessingContext constant buffer is 256 bytes");

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

void ValidateRegionDesc(const PyramidRegionBlurDesc& desc, const char* functionName) {
    ValidateFinite(desc.centerX, functionName, "centerX");
    ValidateFinite(desc.centerY, functionName, "centerY");
    ValidateFinite(desc.radius, functionName, "radius");
    ValidateFinite(desc.rectX, functionName, "rectX");
    ValidateFinite(desc.rectY, functionName, "rectY");
    ValidateFinite(desc.rectWidth, functionName, "rectWidth");
    ValidateFinite(desc.rectHeight, functionName, "rectHeight");
    ValidateFinite(desc.edgeSoftness, functionName, "edgeSoftness");
    ValidateRange01(desc.blurStrength, functionName, "blurStrength");

    if (desc.edgeSoftness < 0.0f) {
        throw ValidationError("D3D11PyramidRegionBlur::DispatchPyramidRegionBlur: edgeSoftness must be >= 0");
    }

    switch (desc.shape) {
    case RegionShape::Circle:
        if (!(desc.radius > 0.0f)) {
            throw ValidationError("D3D11PyramidRegionBlur::DispatchPyramidRegionBlur: radius must be > 0 for circle region");
        }
        break;
    case RegionShape::Rect:
        if (!(desc.rectWidth > 0.0f) || !(desc.rectHeight > 0.0f)) {
            throw ValidationError("D3D11PyramidRegionBlur::DispatchPyramidRegionBlur: rectWidth and rectHeight must be > 0 for rect region");
        }
        break;
    default:
        throw ValidationError("D3D11PyramidRegionBlur::DispatchPyramidRegionBlur: unsupported region shape");
    }

    switch (desc.selection) {
    case RegionSelection::Inside:
    case RegionSelection::Outside:
        break;
    default:
        throw ValidationError("D3D11PyramidRegionBlur::DispatchPyramidRegionBlur: unsupported region selection");
    }
}

D3D11RegionBlurBlendConstants MakeBlendConstants(
    const ProcessingRect& srcRect,
    const ProcessingRect& dstRect,
    const PyramidRegionBlurDesc& desc) {

    D3D11RegionBlurBlendConstants c = {};
    c.srcWidth = srcRect.width;
    c.srcHeight = srcRect.height;
    c.dstWidth = dstRect.width;
    c.dstHeight = dstRect.height;
    c.srcX = srcRect.x;
    c.srcY = srcRect.y;
    c.dstX = dstRect.x;
    c.dstY = dstRect.y;
    c.shape = static_cast<UINT>(desc.shape);
    c.selection = static_cast<UINT>(desc.selection);
    c.centerX = desc.centerX;
    c.centerY = desc.centerY;
    c.radius = desc.radius;
    c.edgeSoftness = desc.edgeSoftness;
    c.rectX = desc.rectX;
    c.rectY = desc.rectY;
    c.rectWidth = desc.rectWidth;
    c.rectHeight = desc.rectHeight;
    c.blurStrength = desc.blurStrength;
    return c;
}

void BindAndDispatchBlend(
    D3D11ProcessingContext& context,
    const D3D11ComputePipeline& pipeline,
    ID3D11DeviceContext* deviceContext,
    ID3D11ShaderResourceView* originalSrv,
    ID3D11ShaderResourceView* blurredSrv,
    ID3D11UnorderedAccessView* dstUav,
    const D3D11RegionBlurBlendConstants& constants) {

    if (!deviceContext) {
        throw ValidationError("D3D11PyramidRegionBlur::DispatchPyramidRegionBlur: null device context");
    }

    context.UpdateConstants(deviceContext, &constants, static_cast<UINT>(sizeof(constants)));

    ID3D11Buffer* cb = context.ConstantBuffer();
    deviceContext->CSSetConstantBuffers(0, 1, &cb);

    ID3D11ShaderResourceView* srvs[] = { originalSrv, blurredSrv };
    deviceContext->CSSetShaderResources(0, 2, srvs);

    UINT initialCounts[] = { static_cast<UINT>(-1) };
    ID3D11UnorderedAccessView* uavs[] = { dstUav };
    deviceContext->CSSetUnorderedAccessViews(0, 1, uavs, initialCounts);

    pipeline.Bind(deviceContext);
    deviceContext->Dispatch(
        DivideRoundUp(constants.dstWidth, kThreadGroupX),
        DivideRoundUp(constants.dstHeight, kThreadGroupY),
        1);

    ID3D11ShaderResourceView* nullSrvs[] = { nullptr, nullptr };
    ID3D11UnorderedAccessView* nullUavs[] = { nullptr };
    ID3D11Buffer* nullCb = nullptr;
    deviceContext->CSSetShaderResources(0, 2, nullSrvs);
    deviceContext->CSSetUnorderedAccessViews(0, 1, nullUavs, nullptr);
    deviceContext->CSSetConstantBuffers(0, 1, &nullCb);
    pipeline.Unbind(deviceContext);
}

} // namespace

struct D3D11PyramidRegionBlur::Pipelines {
    D3D11ComputePipeline blend;
    bool initialized = false;
};

D3D11PyramidRegionBlur::D3D11PyramidRegionBlur() = default;
D3D11PyramidRegionBlur::~D3D11PyramidRegionBlur() = default;
D3D11PyramidRegionBlur::D3D11PyramidRegionBlur(D3D11PyramidRegionBlur&&) noexcept = default;
D3D11PyramidRegionBlur& D3D11PyramidRegionBlur::operator=(D3D11PyramidRegionBlur&&) noexcept = default;

void D3D11PyramidRegionBlur::Initialize(D3D11ProcessingContext& context) {
    m_context = &context;
    m_shaderCache.Initialize(context);
    m_pyramidBlur.Initialize(context);
    m_pipelines.reset();
}

void D3D11PyramidRegionBlur::EnsureInitialized() const {
    if (!m_context) {
        throw ValidationError("D3D11PyramidRegionBlur: processor is not initialized");
    }
}

void D3D11PyramidRegionBlur::EnsurePipelines() {
    EnsureInitialized();

    if (!m_pipelines) {
        m_pipelines.reset(new Pipelines());
    }

    if (m_pipelines->initialized) {
        return;
    }

    m_pipelines->blend.Initialize(
        m_context->GetDevice(),
        m_shaderCache.GetComputeShader("RegionBlurBlendRgba.hlsl"));
    m_pipelines->initialized = true;
}

D3D11PyramidRegionBlurWorkspace D3D11PyramidRegionBlur::CreateWorkspace(
    D3D11Core& core,
    UINT width,
    UINT height,
    DXGI_FORMAT format,
    UINT levels) {

    EnsureInitialized();

    D3D11PyramidRegionBlurWorkspace workspace;
    workspace.sourceWidth = width;
    workspace.sourceHeight = height;
    workspace.levels = levels;
    workspace.format = format;
    workspace.blurWorkspace = m_pyramidBlur.CreateWorkspace(core, width, height, format, levels);
    workspace.blurred = m_pyramidBlur.CreateOutputTexture(core, width, height, format);
    return workspace;
}

D3D11Resource D3D11PyramidRegionBlur::CreateOutputTexture(
    D3D11Core& core,
    UINT width,
    UINT height,
    DXGI_FORMAT format) {

    EnsureInitialized();
    return m_pyramidBlur.CreateOutputTexture(core, width, height, format);
}

void D3D11PyramidRegionBlur::DispatchPyramidRegionBlur(
    ID3D11DeviceContext* deviceContext,
    D3D11Resource& src,
    D3D11PyramidRegionBlurWorkspace& workspace,
    D3D11Resource& dst,
    const PyramidRegionBlurDesc& desc) {

    EnsurePipelines();
    constexpr const char* kFunction = "D3D11PyramidRegionBlur::DispatchPyramidRegionBlur";

    ValidateTexture2D(src, kFunction, "src");
    ValidateTexture2D(workspace.blurred, kFunction, "workspace.blurred");
    ValidateTexture2D(dst, kFunction, "dst");
    ValidateNotSameResource(src, workspace.blurred, kFunction);
    ValidateNotSameResource(src, dst, kFunction);
    ValidateNotSameResource(workspace.blurred, dst, kFunction);
    ValidateHasSrv(src, kFunction, "src");
    ValidateHasSrv(workspace.blurred, kFunction, "workspace.blurred");
    ValidateHasUav(workspace.blurred, kFunction, "workspace.blurred");
    ValidateHasUav(dst, kFunction, "dst");

    const DXGI_FORMAT srcFormat = ResolveFormat(desc.srcFormat, src);
    const DXGI_FORMAT dstFormat = ResolveFormat(desc.dstFormat, dst);
    if (!IsRgbaLikeFormat(srcFormat) || !IsRgbaLikeFormat(dstFormat) || !IsRgbaLikeFormat(workspace.blurred.GetFormat())) {
        throw UnsupportedFormatError("D3D11PyramidRegionBlur::DispatchPyramidRegionBlur: only RGBA-like formats are supported");
    }
    ValidateRgbaUavSupport(*m_context, dstFormat, kFunction);
    ValidateRgbaUavSupport(*m_context, workspace.blurred.GetFormat(), kFunction);

    const auto srcTexDesc = src.GetTexture2DDesc();
    const auto dstTexDesc = dst.GetTexture2DDesc();
    const ProcessingRect srcRect = ResolveRect(desc.srcRect, srcTexDesc.Width, srcTexDesc.Height);
    const ProcessingRect dstRect = ResolveRect(desc.dstRect, dstTexDesc.Width, dstTexDesc.Height);

    ValidateRectInside(srcRect, srcTexDesc.Width, srcTexDesc.Height, kFunction, "srcRect");
    ValidateRectInside(dstRect, dstTexDesc.Width, dstTexDesc.Height, kFunction, "dstRect");

    if (srcRect.width != dstRect.width || srcRect.height != dstRect.height) {
        throw ValidationError("D3D11PyramidRegionBlur::DispatchPyramidRegionBlur: region blur does not resize; srcRect and dstRect sizes must match");
    }
    if (workspace.sourceWidth != srcRect.width || workspace.sourceHeight != srcRect.height || workspace.levels != desc.levels) {
        throw ValidationError("D3D11PyramidRegionBlur::DispatchPyramidRegionBlur: workspace does not match processing size or levels");
    }

    const auto blurredDesc = workspace.blurred.GetTexture2DDesc();
    if (blurredDesc.Width < dstRect.width || blurredDesc.Height < dstRect.height) {
        throw ValidationError("D3D11PyramidRegionBlur::DispatchPyramidRegionBlur: workspace.blurred is smaller than processing rect");
    }

    ValidateRegionDesc(desc, kFunction);

    PyramidBlurDesc blur = {};
    blur.srcFormat = srcFormat;
    blur.dstFormat = workspace.blurred.GetFormat();
    blur.srcRect = srcRect;
    blur.dstRect = { 0, 0, dstRect.width, dstRect.height };
    blur.levels = desc.levels;
    blur.blurMode = desc.blurMode;
    blur.blurRadius = desc.blurRadius;
    blur.blurSigma = desc.blurSigma;
    blur.blurEdgeMode = desc.blurEdgeMode;
    std::copy(desc.blurBorderColor, desc.blurBorderColor + 4, blur.blurBorderColor);
    blur.upsampleFilter = desc.upsampleFilter;
    blur.pyramidEdgeMode = desc.pyramidEdgeMode;
    std::copy(desc.pyramidBorderColor, desc.pyramidBorderColor + 4, blur.pyramidBorderColor);

    m_pyramidBlur.DispatchPyramidBlur(deviceContext, src, workspace.blurWorkspace, workspace.blurred, blur);

    auto srcView = CreateRgbaTextureViewSet(*m_context, src, true, false, srcFormat);
    auto blurredView = CreateRgbaTextureViewSet(*m_context, workspace.blurred, true, false, workspace.blurred.GetFormat());
    auto dstView = CreateRgbaTextureViewSet(*m_context, dst, false, true, dstFormat);

    const auto constants = MakeBlendConstants(srcRect, dstRect, desc);
    BindAndDispatchBlend(
        *m_context,
        m_pipelines->blend,
        deviceContext,
        srcView.srv.Get(),
        blurredView.srv.Get(),
        dstView.uav.Get(),
        constants);
}

} // namespace Processing
} // namespace D3D11CoreLib
