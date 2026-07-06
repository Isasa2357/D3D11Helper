#pragma once
//
// D3D11ProcessingTypes.hpp
// Common public types for the D3D11 Processing Layer.
//
#include <D3D11Helper/D3D11Core/D3D11Common.hpp>

#include <stdexcept>
#include <string>

namespace D3D11CoreLib {
namespace Processing {

struct ProcessingRect {
    INT x = 0;
    INT y = 0;
    UINT width = 0;
    UINT height = 0;
};

enum class ProcessingFilter : UINT {
    Point = 0,
    Linear = 1,
};

enum class ProcessingColorMatrix : UINT {
    Identity = 0,
    BT601 = 1,
    BT709 = 2,
    BT2020 = 3,
};

enum class ProcessingColorRange : UINT {
    Full = 0,
    Limited = 1,
};

enum class ProcessingAlphaMode : UINT {
    Ignore = 0,
    Preserve = 1,
    Premultiplied = 2,
};

enum class RemapCoordinateMode : UINT {
    AbsolutePixels = 0,
    NormalizedZeroToOne = 1,
};

enum class RemapBorderMode : UINT {
    Clamp = 0,
    Constant = 1,
};

enum class CompositeBlendMode : UINT {
    Copy = 0,
    AlphaBlend = 1,
    PremultipliedAlpha = 2,
    Add = 3,
};

enum class BlurMode : UINT {
    Box = 0,
    Gaussian = 1,
};

enum class BlurEdgeMode : UINT {
    Clamp = 0,
    Constant = 1,
};

enum class RegionShape : UINT {
    Circle = 0,
    Rect = 1,
};

enum class RegionSelection : UINT {
    Inside = 0,
    Outside = 1,
};

enum class RegionEffectMode : UINT {
    Copy = 0,
    Darken = 1,
    Tint = 2,
    Grayscale = 3,
    Highlight = 4,
    AlphaFade = 5,
    Vignette = 6,
};

enum class KernelFilterMode : UINT {
    Custom3x3 = 0,
    Sharpen = 1,
    EdgeDetect = 2,
};

enum class KernelEdgeMode : UINT {
    Clamp = 0,
    Constant = 1,
};

struct ProcessingColorDesc {
    ProcessingColorMatrix srcMatrix = ProcessingColorMatrix::BT709;
    ProcessingColorRange  srcRange  = ProcessingColorRange::Full;
    ProcessingColorMatrix dstMatrix = ProcessingColorMatrix::BT709;
    ProcessingColorRange  dstRange  = ProcessingColorRange::Full;
    ProcessingAlphaMode   alphaMode = ProcessingAlphaMode::Ignore;
};

struct FormatConvertDesc {
    DXGI_FORMAT srcFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT dstFormat = DXGI_FORMAT_UNKNOWN;
    ProcessingColorDesc color = {};
    ProcessingRect srcRect = {};
    ProcessingRect dstRect = {};
};

struct ResizeDesc {
    ProcessingFilter filter = ProcessingFilter::Linear;
    ProcessingRect srcRect = {};
    ProcessingRect dstRect = {};
};

struct FusedConvertResizeDesc {
    DXGI_FORMAT srcFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT dstFormat = DXGI_FORMAT_UNKNOWN;
    ProcessingColorDesc color = {};
    ProcessingFilter filter = ProcessingFilter::Linear;
    ProcessingRect srcRect = {};
    ProcessingRect dstRect = {};
};

struct RemapDesc {
    DXGI_FORMAT srcFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT dstFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT mapFormat = DXGI_FORMAT_R32G32_FLOAT;
    ProcessingFilter filter = ProcessingFilter::Linear;
    ProcessingRect dstRect = {};
    RemapCoordinateMode coordinateMode = RemapCoordinateMode::AbsolutePixels;
    RemapBorderMode borderMode = RemapBorderMode::Clamp;
    float borderColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};

struct CompositeDesc {
    DXGI_FORMAT baseFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT overlayFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT dstFormat = DXGI_FORMAT_UNKNOWN;
    ProcessingRect baseRect = {};
    ProcessingRect overlayRect = {};
    ProcessingRect dstRect = {};
    CompositeBlendMode blendMode = CompositeBlendMode::AlphaBlend;
    float opacity = 1.0f;
};

struct BlurDesc {
    BlurMode mode = BlurMode::Gaussian;
    ProcessingRect srcRect = {};
    ProcessingRect dstRect = {};
    UINT radius = 5;
    float sigma = 2.0f;
    BlurEdgeMode edgeMode = BlurEdgeMode::Clamp;
    float borderColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};

struct RegionEffectDesc {
    DXGI_FORMAT srcFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT dstFormat = DXGI_FORMAT_UNKNOWN;
    ProcessingRect srcRect = {};
    ProcessingRect dstRect = {};

    RegionShape shape = RegionShape::Circle;
    RegionSelection selection = RegionSelection::Outside;
    RegionEffectMode effect = RegionEffectMode::Darken;

    // Circle parameters are in destination texture pixel coordinates.
    float centerX = 0.0f;
    float centerY = 0.0f;
    float radius = 1.0f;

    // Rect parameters are in destination texture pixel coordinates.
    float rectX = 0.0f;
    float rectY = 0.0f;
    float rectWidth = 1.0f;
    float rectHeight = 1.0f;

    // Soft transition width in pixels. 0 gives a hard edge.
    float edgeSoftness = 0.0f;

    // Darken/Vignette: RGB is multiplied by lerp(1, darkenFactor, mask).
    float darkenFactor = 0.5f;

    // Tint: RGB is lerped toward tintColor.rgb by tintStrength * mask.
    float tintColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float tintStrength = 0.25f;

    // Grayscale: RGB is lerped toward luma by grayscaleStrength * mask.
    float grayscaleStrength = 1.0f;

    // Highlight: RGB is boosted by highlightColor.rgb * highlightStrength * mask.
    float highlightColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float highlightStrength = 0.25f;

    // AlphaFade: alpha is multiplied by lerp(1, alphaFactor, mask).
    float alphaFactor = 0.0f;

    // Vignette: mask is additionally scaled by vignetteStrength.
    float vignetteStrength = 1.0f;
};

struct RegionBlurDesc {
    DXGI_FORMAT srcFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT dstFormat = DXGI_FORMAT_UNKNOWN;
    ProcessingRect srcRect = {};
    ProcessingRect dstRect = {};

    RegionShape shape = RegionShape::Circle;
    RegionSelection selection = RegionSelection::Outside;

    // Circle parameters are in destination texture pixel coordinates.
    float centerX = 0.0f;
    float centerY = 0.0f;
    float radius = 1.0f;

    // Rect parameters are in destination texture pixel coordinates.
    float rectX = 0.0f;
    float rectY = 0.0f;
    float rectWidth = 1.0f;
    float rectHeight = 1.0f;

    // Soft transition width in pixels. 0 gives a hard edge.
    float edgeSoftness = 0.0f;

    // Final blend amount between original and blurred image inside the selected region.
    float blurStrength = 1.0f;

    BlurMode blurMode = BlurMode::Gaussian;
    UINT blurRadius = 5;
    float blurSigma = 2.0f;
    BlurEdgeMode blurEdgeMode = BlurEdgeMode::Clamp;
    float blurBorderColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};



struct ColorAdjustDesc {
    DXGI_FORMAT srcFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT dstFormat = DXGI_FORMAT_UNKNOWN;
    ProcessingRect srcRect = {};
    ProcessingRect dstRect = {};

    // RGB is adjusted in normalized [0, 1] space.
    // brightness is additive, contrast/saturation are multipliers, gamma must be > 0.
    float brightness = 0.0f;
    float contrast = 1.0f;
    float gamma = 1.0f;
    float saturation = 1.0f;
};

struct KernelFilterDesc {
    DXGI_FORMAT srcFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT dstFormat = DXGI_FORMAT_UNKNOWN;
    ProcessingRect srcRect = {};
    ProcessingRect dstRect = {};

    KernelFilterMode mode = KernelFilterMode::Sharpen;
    KernelEdgeMode edgeMode = KernelEdgeMode::Clamp;

    // Used only when mode == Custom3x3. Row-major 3x3 kernel.
    float kernel[9] = {
        0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f,
    };

    float scale = 1.0f;
    float bias = 0.0f;
    bool preserveAlpha = true;
    float borderColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};

struct D3D11ProcessingCaps {

    bool rgba8Uav = false;
    bool bgra8Uav = false;
    bool rgba16FloatUav = false;

    bool nv12Srv = false;
    bool nv12Uav = false;
    bool p010Srv = false;
    bool p010Uav = false;

    bool r8Uav = false;
    bool rg8Uav = false;
    bool r16Uav = false;
    bool rg16Uav = false;
};

class ProcessingError : public std::runtime_error {
public:
    explicit ProcessingError(const std::string& message) : std::runtime_error(message) {}
};

class UnsupportedFormatError : public ProcessingError {
public:
    explicit UnsupportedFormatError(const std::string& message) : ProcessingError(message) {}
};

class UnsupportedFeatureError : public ProcessingError {
public:
    explicit UnsupportedFeatureError(const std::string& message) : ProcessingError(message) {}
};

class ValidationError : public ProcessingError {
public:
    explicit ValidationError(const std::string& message) : ProcessingError(message) {}
};

bool IsRgbaLikeFormat(DXGI_FORMAT format) noexcept;
bool IsYuv420Format(DXGI_FORMAT format) noexcept;
bool IsSupportedProcessingFormat(DXGI_FORMAT format) noexcept;
bool IsSupportedRgbaOutputFormat(DXGI_FORMAT format) noexcept;
bool IsSupportedRemapMapFormat(DXGI_FORMAT format) noexcept;
bool IsSupportedCompositeFormat(DXGI_FORMAT format) noexcept;

ProcessingRect ResolveRect(const ProcessingRect& rect, UINT fallbackWidth, UINT fallbackHeight);
void ValidateRectInside(const ProcessingRect& rect, UINT width, UINT height, const char* functionName, const char* argumentName);
void ValidateEvenSize(UINT width, UINT height, DXGI_FORMAT format, const char* functionName);
void ValidateYuv420Rect(const ProcessingRect& rect, const char* functionName, const char* argumentName);
void ValidateOpacity(float opacity, const char* functionName);

} // namespace Processing
} // namespace D3D11CoreLib
