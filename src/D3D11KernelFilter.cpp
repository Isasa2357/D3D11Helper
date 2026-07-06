#include <D3D11Helper/D3D11Processing/D3D11KernelFilter.hpp>
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
constexpr UINT kPackedKernelCount = 12;

struct D3D11KernelFilterConstants {
    UINT srcWidth = 0;
    UINT srcHeight = 0;
    UINT dstWidth = 0;
    UINT dstHeight = 0;

    INT srcX = 0;
    INT srcY = 0;
    INT dstX = 0;
    INT dstY = 0;

    UINT edgeMode = 0;
    UINT preserveAlpha = 1;
    UINT reserved0 = 0;
    UINT reserved1 = 0;

    float scale = 1.0f;
    float bias = 0.0f;
    float reserved2 = 0.0f;
    float reserved3 = 0.0f;

    float borderColor[4] = { 0, 0, 0, 0 };
    float kernel[kPackedKernelCount] = {};
};
static_assert((sizeof(D3D11KernelFilterConstants) % 16) == 0, "constant buffer size must be 16-byte aligned");
static_assert(sizeof(D3D11KernelFilterConstants) <= 256, "D3D11ProcessingContext constant buffer is 256 bytes");

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

void ValidateKernelFilterDesc(const KernelFilterDesc& desc, const char* functionName) {
    switch (desc.mode) {
    case KernelFilterMode::Custom3x3:
    case KernelFilterMode::Sharpen:
    case KernelFilterMode::EdgeDetect:
        break;
    default:
        throw ValidationError("D3D11KernelFilter::DispatchKernelFilter: unsupported kernel filter mode");
    }

    switch (desc.edgeMode) {
    case KernelEdgeMode::Clamp:
    case KernelEdgeMode::Constant:
        break;
    default:
        throw ValidationError("D3D11KernelFilter::DispatchKernelFilter: unsupported edge mode");
    }

    ValidateFinite(desc.scale, functionName, "scale");
    ValidateFinite(desc.bias, functionName, "bias");

    for (int i = 0; i < 9; ++i) {
        ValidateFinite(desc.kernel[i], functionName, "kernel");
    }
    for (int i = 0; i < 4; ++i) {
        ValidateFinite(desc.borderColor[i], functionName, "borderColor");
    }
}

void FillKernel(const KernelFilterDesc& desc, float* kernel) {
    std::fill(kernel, kernel + kPackedKernelCount, 0.0f);

    switch (desc.mode) {
    case KernelFilterMode::Custom3x3:
        std::copy(desc.kernel, desc.kernel + 9, kernel);
        break;
    case KernelFilterMode::Sharpen: {
        const float k[9] = {
             0.0f, -1.0f,  0.0f,
            -1.0f,  5.0f, -1.0f,
             0.0f, -1.0f,  0.0f,
        };
        std::copy(k, k + 9, kernel);
        break;
    }
    case KernelFilterMode::EdgeDetect: {
        const float k[9] = {
            -1.0f, -1.0f, -1.0f,
            -1.0f,  8.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
        };
        std::copy(k, k + 9, kernel);
        break;
    }
    default:
        break;
    }
}

D3D11KernelFilterConstants MakeKernelFilterConstants(
    const ProcessingRect& srcRect,
    const ProcessingRect& dstRect,
    const KernelFilterDesc& desc) {

    D3D11KernelFilterConstants c = {};
    c.srcWidth = srcRect.width;
    c.srcHeight = srcRect.height;
    c.dstWidth = dstRect.width;
    c.dstHeight = dstRect.height;
    c.srcX = srcRect.x;
    c.srcY = srcRect.y;
    c.dstX = dstRect.x;
    c.dstY = dstRect.y;
    c.edgeMode = static_cast<UINT>(desc.edgeMode);
    c.preserveAlpha = desc.preserveAlpha ? 1u : 0u;
    c.scale = desc.scale;
    c.bias = desc.bias;
    std::copy(desc.borderColor, desc.borderColor + 4, c.borderColor);
    FillKernel(desc, c.kernel);
    return c;
}

void BindAndDispatch(
    D3D11ProcessingContext& context,
    const D3D11ComputePipeline& pipeline,
    ID3D11DeviceContext* deviceContext,
    ID3D11ShaderResourceView* srv,
    ID3D11UnorderedAccessView* uav,
    const D3D11KernelFilterConstants& constants) {

    if (!deviceContext) {
        throw ValidationError("D3D11KernelFilter::DispatchKernelFilter: null device context");
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

struct D3D11KernelFilter::Pipelines {
    D3D11ComputePipeline kernelFilter;
    bool initialized = false;
};

D3D11KernelFilter::D3D11KernelFilter() = default;
D3D11KernelFilter::~D3D11KernelFilter() = default;
D3D11KernelFilter::D3D11KernelFilter(D3D11KernelFilter&&) noexcept = default;
D3D11KernelFilter& D3D11KernelFilter::operator=(D3D11KernelFilter&&) noexcept = default;

void D3D11KernelFilter::Initialize(D3D11ProcessingContext& context) {
    m_context = &context;
    m_shaderCache.Initialize(context);
    m_pipelines.reset();
}

void D3D11KernelFilter::EnsureInitialized() const {
    if (!m_context) {
        throw ValidationError("D3D11KernelFilter: filter is not initialized");
    }
}

void D3D11KernelFilter::EnsurePipelines() {
    EnsureInitialized();

    if (!m_pipelines) {
        m_pipelines.reset(new Pipelines());
    }

    if (m_pipelines->initialized) {
        return;
    }

    m_pipelines->kernelFilter.Initialize(m_context->GetDevice(), m_shaderCache.GetComputeShader("KernelFilterRgba.hlsl"));
    m_pipelines->initialized = true;
}

void D3D11KernelFilter::DispatchKernelFilter(
    ID3D11DeviceContext* deviceContext,
    D3D11Resource& src,
    D3D11Resource& dst,
    const KernelFilterDesc& desc) {

    EnsurePipelines();

    constexpr const char* kFunction = "D3D11KernelFilter::DispatchKernelFilter";

    ValidateTexture2D(src, kFunction, "src");
    ValidateTexture2D(dst, kFunction, "dst");
    ValidateNotSameResource(src, dst, kFunction);
    ValidateHasSrv(src, kFunction, "src");
    ValidateHasUav(dst, kFunction, "dst");
    ValidateKernelFilterDesc(desc, kFunction);

    const DXGI_FORMAT srcFormat = ResolveFormat(desc.srcFormat, src);
    const DXGI_FORMAT dstFormat = ResolveFormat(desc.dstFormat, dst);

    if (!IsRgbaLikeFormat(srcFormat) || !IsRgbaLikeFormat(dstFormat)) {
        throw UnsupportedFormatError("D3D11KernelFilter::DispatchKernelFilter: only RGBA-like formats are supported");
    }

    ValidateRgbaUavSupport(*m_context, dstFormat, kFunction);

    const auto srcDesc = src.GetTexture2DDesc();
    const auto dstDesc = dst.GetTexture2DDesc();

    const ProcessingRect srcRect = ResolveRect(desc.srcRect, srcDesc.Width, srcDesc.Height);
    const ProcessingRect dstRect = ResolveRect(desc.dstRect, dstDesc.Width, dstDesc.Height);

    ValidateRectInside(srcRect, srcDesc.Width, srcDesc.Height, kFunction, "srcRect");
    ValidateRectInside(dstRect, dstDesc.Width, dstDesc.Height, kFunction, "dstRect");

    if (srcRect.width != dstRect.width || srcRect.height != dstRect.height) {
        throw ValidationError("D3D11KernelFilter::DispatchKernelFilter: kernel filter does not resize; srcRect and dstRect sizes must match");
    }

    const auto constants = MakeKernelFilterConstants(srcRect, dstRect, desc);

    auto srcView = CreateRgbaTextureViewSet(*m_context, src, true, false, srcFormat);
    auto dstView = CreateRgbaTextureViewSet(*m_context, dst, false, true, dstFormat);

    BindAndDispatch(
        *m_context,
        m_pipelines->kernelFilter,
        deviceContext,
        srcView.srv.Get(),
        dstView.uav.Get(),
        constants);
}

D3D11Resource D3D11KernelFilter::CreateOutputTexture(
    D3D11Core& core,
    UINT width,
    UINT height,
    DXGI_FORMAT format) {

    EnsureInitialized();

    if (width == 0 || height == 0) {
        throw ValidationError("D3D11KernelFilter::CreateOutputTexture: size is zero");
    }

    if (!IsRgbaLikeFormat(format)) {
        throw UnsupportedFormatError("D3D11KernelFilter::CreateOutputTexture: only RGBA-like formats are supported");
    }

    ValidateRgbaUavSupport(*m_context, format, "D3D11KernelFilter::CreateOutputTexture");

    return CreateTexture2D(
        core,
        width,
        height,
        format,
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
}

} // namespace Processing
} // namespace D3D11CoreLib
