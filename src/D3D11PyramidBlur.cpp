#include <D3D11Helper/D3D11Processing/D3D11PyramidBlur.hpp>
#include <D3D11Helper/D3D11Framework/D3D11Helpers.hpp>

#include <algorithm>
#include <sstream>
#include <string>
#include <utility>

namespace D3D11CoreLib {
namespace Processing {
namespace {

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

void ValidateLevels(UINT levels, const char* functionName) {
    if (levels == 0 || levels > D3D11PyramidBlur::MaxLevels) {
        std::ostringstream os;
        os << functionName << ": levels must be in [1, " << D3D11PyramidBlur::MaxLevels << "]";
        throw ValidationError(os.str());
    }
}

void ValidateDivisibleByPyramidLevels(UINT width, UINT height, UINT levels, const char* functionName) {
    const UINT divisor = 1u << levels;
    if ((width % divisor) != 0 || (height % divisor) != 0) {
        std::ostringstream os;
        os << functionName << ": processing rect size must be divisible by 2^levels; size="
           << width << "x" << height << " levels=" << levels;
        throw ValidationError(os.str());
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

void ValidatePyramidEdgeMode(PyramidEdgeMode mode, const char* functionName) {
    switch (mode) {
    case PyramidEdgeMode::Clamp:
    case PyramidEdgeMode::Constant:
        return;
    default:
        throw ValidationError(std::string(functionName) + ": unsupported pyramid edge mode");
    }
}

void ValidateWorkspaceResource(
    const D3D11Resource& resource,
    UINT width,
    UINT height,
    DXGI_FORMAT format,
    const char* functionName,
    const char* argumentName) {

    ValidateTexture2D(resource, functionName, argumentName);
    ValidateHasSrv(resource, functionName, argumentName);
    ValidateHasUav(resource, functionName, argumentName);

    const auto desc = resource.GetTexture2DDesc();
    if (desc.Width < width || desc.Height < height) {
        std::ostringstream os;
        os << functionName << ": " << argumentName << " is too small; expected at least "
           << width << "x" << height << " actual " << desc.Width << "x" << desc.Height;
        throw ValidationError(os.str());
    }

    if (resource.GetFormat() != format) {
        std::ostringstream os;
        os << functionName << ": " << argumentName << " format does not match workspace format";
        throw ValidationError(os.str());
    }
}

} // namespace

D3D11PyramidBlur::D3D11PyramidBlur() = default;
D3D11PyramidBlur::~D3D11PyramidBlur() = default;
D3D11PyramidBlur::D3D11PyramidBlur(D3D11PyramidBlur&&) noexcept = default;
D3D11PyramidBlur& D3D11PyramidBlur::operator=(D3D11PyramidBlur&&) noexcept = default;

void D3D11PyramidBlur::Initialize(D3D11ProcessingContext& context) {
    m_context = &context;
    m_pyramid.Initialize(context);
    m_blurrer.Initialize(context);
}

void D3D11PyramidBlur::EnsureInitialized() const {
    if (!m_context) {
        throw ValidationError("D3D11PyramidBlur: processor is not initialized");
    }
}

D3D11PyramidBlurWorkspace D3D11PyramidBlur::CreateWorkspace(
    D3D11Core& core,
    UINT width,
    UINT height,
    DXGI_FORMAT format,
    UINT levels) {

    EnsureInitialized();
    constexpr const char* kFunction = "D3D11PyramidBlur::CreateWorkspace";

    if (width == 0 || height == 0) {
        throw ValidationError("D3D11PyramidBlur::CreateWorkspace: size is zero");
    }
    if (!IsRgbaLikeFormat(format)) {
        throw UnsupportedFormatError("D3D11PyramidBlur::CreateWorkspace: only RGBA-like formats are supported");
    }

    ValidateLevels(levels, kFunction);
    ValidateDivisibleByPyramidLevels(width, height, levels, kFunction);
    ValidateRgbaUavSupport(*m_context, format, kFunction);

    D3D11PyramidBlurWorkspace workspace;
    workspace.sourceWidth = width;
    workspace.sourceHeight = height;
    workspace.levels = levels;
    workspace.format = format;

    UINT w = width;
    UINT h = height;
    workspace.downTextures.reserve(levels);
    for (UINT i = 0; i < levels; ++i) {
        w = HalfRoundUp(w);
        h = HalfRoundUp(h);
        workspace.downTextures.emplace_back(m_pyramid.CreateOutputTexture(core, w, h, format));
    }

    workspace.blurScratch = m_blurrer.CreateScratchTexture(core, w, h, format);
    workspace.blurredLow = m_blurrer.CreateOutputTexture(core, w, h, format);

    if (levels > 1) {
        workspace.upTextures.reserve(levels - 1u);
        for (UINT i = 0; i < levels - 1u; ++i) {
            const auto desc = workspace.downTextures[i].GetTexture2DDesc();
            workspace.upTextures.emplace_back(m_pyramid.CreateOutputTexture(core, desc.Width, desc.Height, format));
        }
    }

    return workspace;
}

D3D11Resource D3D11PyramidBlur::CreateOutputTexture(
    D3D11Core& core,
    UINT width,
    UINT height,
    DXGI_FORMAT format) {

    EnsureInitialized();
    return m_pyramid.CreateOutputTexture(core, width, height, format);
}

void D3D11PyramidBlur::DispatchPyramidBlur(
    ID3D11DeviceContext* deviceContext,
    D3D11Resource& src,
    D3D11PyramidBlurWorkspace& workspace,
    D3D11Resource& dst,
    const PyramidBlurDesc& desc) {

    EnsureInitialized();
    constexpr const char* kFunction = "D3D11PyramidBlur::DispatchPyramidBlur";

    ValidateTexture2D(src, kFunction, "src");
    ValidateTexture2D(dst, kFunction, "dst");
    ValidateNotSameResource(src, dst, kFunction);
    ValidateHasSrv(src, kFunction, "src");
    ValidateHasUav(dst, kFunction, "dst");

    ValidateLevels(desc.levels, kFunction);
    ValidateProcessingFilter(desc.upsampleFilter, kFunction);
    ValidatePyramidEdgeMode(desc.pyramidEdgeMode, kFunction);

    const DXGI_FORMAT srcFormat = ResolveFormat(desc.srcFormat, src);
    const DXGI_FORMAT dstFormat = ResolveFormat(desc.dstFormat, dst);
    ValidateRgbaFormats(*m_context, srcFormat, dstFormat, kFunction);

    if (workspace.levels != desc.levels) {
        throw ValidationError("D3D11PyramidBlur::DispatchPyramidBlur: workspace levels do not match desc.levels");
    }
    if (workspace.format != dstFormat) {
        throw ValidationError("D3D11PyramidBlur::DispatchPyramidBlur: workspace format does not match dst format");
    }
    if (workspace.downTextures.size() != desc.levels || workspace.upTextures.size() != (desc.levels - 1u)) {
        throw ValidationError("D3D11PyramidBlur::DispatchPyramidBlur: invalid workspace texture count");
    }

    const auto srcTexDesc = src.GetTexture2DDesc();
    const auto dstTexDesc = dst.GetTexture2DDesc();
    const ProcessingRect srcRect = ResolveRect(desc.srcRect, srcTexDesc.Width, srcTexDesc.Height);
    const ProcessingRect dstRect = ResolveRect(desc.dstRect, dstTexDesc.Width, dstTexDesc.Height);

    ValidateRectInside(srcRect, srcTexDesc.Width, srcTexDesc.Height, kFunction, "srcRect");
    ValidateRectInside(dstRect, dstTexDesc.Width, dstTexDesc.Height, kFunction, "dstRect");

    if (srcRect.width != dstRect.width || srcRect.height != dstRect.height) {
        throw ValidationError("D3D11PyramidBlur::DispatchPyramidBlur: pyramid blur does not resize; srcRect and dstRect sizes must match");
    }

    ValidateDivisibleByPyramidLevels(srcRect.width, srcRect.height, desc.levels, kFunction);

    if (workspace.sourceWidth != srcRect.width || workspace.sourceHeight != srcRect.height) {
        throw ValidationError("D3D11PyramidBlur::DispatchPyramidBlur: workspace size does not match processing rect size");
    }

    D3D11Resource* current = &src;
    ProcessingRect currentRect = srcRect;

    for (UINT i = 0; i < desc.levels; ++i) {
        const UINT nextW = currentRect.width / 2u;
        const UINT nextH = currentRect.height / 2u;
        ValidateWorkspaceResource(workspace.downTextures[i], nextW, nextH, dstFormat, kFunction, "downTextures");

        PyramidDownsampleDesc down = {};
        down.srcFormat = (i == 0) ? srcFormat : dstFormat;
        down.dstFormat = dstFormat;
        down.srcRect = currentRect;
        down.dstRect = { 0, 0, nextW, nextH };
        down.edgeMode = desc.pyramidEdgeMode;
        std::copy(desc.pyramidBorderColor, desc.pyramidBorderColor + 4, down.borderColor);

        m_pyramid.DispatchDownsample2x(deviceContext, *current, workspace.downTextures[i], down);
        current = &workspace.downTextures[i];
        currentRect = { 0, 0, nextW, nextH };
    }

    ValidateWorkspaceResource(workspace.blurScratch, currentRect.width, currentRect.height, dstFormat, kFunction, "blurScratch");
    ValidateWorkspaceResource(workspace.blurredLow, currentRect.width, currentRect.height, dstFormat, kFunction, "blurredLow");

    BlurDesc blur = {};
    blur.mode = desc.blurMode;
    blur.srcRect = currentRect;
    blur.dstRect = currentRect;
    blur.radius = desc.blurRadius;
    blur.sigma = desc.blurSigma;
    blur.edgeMode = desc.blurEdgeMode;
    std::copy(desc.blurBorderColor, desc.blurBorderColor + 4, blur.borderColor);

    m_blurrer.DispatchBlur(deviceContext, *current, workspace.blurScratch, workspace.blurredLow, blur);
    current = &workspace.blurredLow;

    for (int level = static_cast<int>(desc.levels) - 1; level >= 0; --level) {
        const UINT nextW = currentRect.width * 2u;
        const UINT nextH = currentRect.height * 2u;

        D3D11Resource* target = nullptr;
        ProcessingRect targetRect = {};
        if (level == 0) {
            target = &dst;
            targetRect = dstRect;
        } else {
            target = &workspace.upTextures[static_cast<size_t>(level - 1)];
            targetRect = { 0, 0, nextW, nextH };
            ValidateWorkspaceResource(*target, nextW, nextH, dstFormat, kFunction, "upTextures");
        }

        PyramidUpsampleDesc up = {};
        up.srcFormat = dstFormat;
        up.dstFormat = dstFormat;
        up.srcRect = currentRect;
        up.dstRect = targetRect;
        up.filter = desc.upsampleFilter;
        up.edgeMode = desc.pyramidEdgeMode;
        std::copy(desc.pyramidBorderColor, desc.pyramidBorderColor + 4, up.borderColor);

        m_pyramid.DispatchUpsample2x(deviceContext, *current, *target, up);
        current = target;
        currentRect = { 0, 0, nextW, nextH };
    }
}

} // namespace Processing
} // namespace D3D11CoreLib
