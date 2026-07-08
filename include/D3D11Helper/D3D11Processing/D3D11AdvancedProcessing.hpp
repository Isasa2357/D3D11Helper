#pragma once
//
// D3D11AdvancedProcessing.hpp
// Advanced Layer 3 processing passes: affine/perspective transform, 3D LUT,
// and undistort-map application.
//
#include <D3D11Helper/D3D11Processing/D3D11ProcessingContext.hpp>
#include <D3D11Helper/D3D11Processing/D3D11ProcessingShaderCache.hpp>
#include <D3D11Helper/D3D11Processing/D3D11Remap.hpp>
#include <D3D11Helper/D3D11Framework/D3D11ComputePipeline.hpp>

#include <memory>

namespace D3D11CoreLib {
namespace Processing {

// dstToSrc maps destination-local pixel coordinates to source-local pixel
// coordinates:
//   sx = m00 * x + m01 * y + m02
//   sy = m10 * x + m11 * y + m12
struct AffineTransformDesc {
    DXGI_FORMAT srcFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT dstFormat = DXGI_FORMAT_UNKNOWN;
    ProcessingFilter filter = ProcessingFilter::Linear;
    ProcessingRect srcRect = {};
    ProcessingRect dstRect = {};
    float dstToSrc[6] = {
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
    };
    RemapBorderMode borderMode = RemapBorderMode::Clamp;
    float borderColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};

// dstToSrc is a row-major 3x3 homography mapping destination-local pixel
// coordinates to source-local pixel coordinates. The shader divides by the
// third row value when it is non-zero.
struct PerspectiveTransformDesc {
    DXGI_FORMAT srcFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT dstFormat = DXGI_FORMAT_UNKNOWN;
    ProcessingFilter filter = ProcessingFilter::Linear;
    ProcessingRect srcRect = {};
    ProcessingRect dstRect = {};
    float dstToSrc[9] = {
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f,
    };
    RemapBorderMode borderMode = RemapBorderMode::Clamp;
    float borderColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};

// 3D LUT pass. The LUT resource must be a Texture3D whose RGB input domain is
// [0, 1]. When lutWidth/Height/Depth are zero, the dimensions are read from the
// resource description. The shader performs manual trilinear interpolation.
struct Lut3DDesc {
    DXGI_FORMAT srcFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT dstFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT lutFormat = DXGI_FORMAT_UNKNOWN;
    ProcessingRect srcRect = {};
    ProcessingRect dstRect = {};
    UINT lutWidth = 0;
    UINT lutHeight = 0;
    UINT lutDepth = 0;
    float strength = 1.0f;
    bool preserveAlpha = true;
};

class D3D11AdvancedProcessor {
public:
    D3D11AdvancedProcessor();
    ~D3D11AdvancedProcessor();

    D3D11AdvancedProcessor(const D3D11AdvancedProcessor&) = delete;
    D3D11AdvancedProcessor& operator=(const D3D11AdvancedProcessor&) = delete;
    D3D11AdvancedProcessor(D3D11AdvancedProcessor&&) noexcept;
    D3D11AdvancedProcessor& operator=(D3D11AdvancedProcessor&&) noexcept;

    void Initialize(D3D11ProcessingContext& context);

    void DispatchAffineTransform(
        ID3D11DeviceContext* deviceContext,
        D3D11Resource& src,
        D3D11Resource& dst,
        const AffineTransformDesc& desc);

    void DispatchPerspectiveTransform(
        ID3D11DeviceContext* deviceContext,
        D3D11Resource& src,
        D3D11Resource& dst,
        const PerspectiveTransformDesc& desc);

    void DispatchApplyLut3D(
        ID3D11DeviceContext* deviceContext,
        D3D11Resource& src,
        D3D11Resource& lut,
        D3D11Resource& dst,
        const Lut3DDesc& desc);

    // Thin alias over D3D11Remapper for camera undistort/remap maps.
    // The map texture must match RemapDesc::mapFormat, currently R32G32_FLOAT.
    void DispatchApplyUndistortMap(
        ID3D11DeviceContext* deviceContext,
        D3D11Resource& src,
        D3D11Resource& map,
        D3D11Resource& dst,
        const RemapDesc& desc);

    D3D11Resource CreateOutputTexture(
        D3D11Core& core,
        UINT width,
        UINT height,
        DXGI_FORMAT format);

private:
    struct Pipelines;

    void EnsureInitialized() const;
    void EnsurePipelines();

    D3D11ProcessingContext* m_context = nullptr;
    D3D11ProcessingShaderCache m_shaderCache;
    std::unique_ptr<Pipelines> m_pipelines;
    D3D11Remapper m_remapper;
};

} // namespace Processing
} // namespace D3D11CoreLib
