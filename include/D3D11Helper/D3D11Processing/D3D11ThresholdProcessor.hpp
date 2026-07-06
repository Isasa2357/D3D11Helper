#pragma once
//
// D3D11ThresholdProcessor.hpp
// Thresholding and visualization passes for RGBA-like textures.
//
#include <D3D11Helper/D3D11Processing/D3D11ProcessingContext.hpp>
#include <D3D11Helper/D3D11Processing/D3D11ProcessingShaderCache.hpp>
#include <D3D11Helper/D3D11Processing/D3D11TextureViews.hpp>
#include <D3D11Helper/D3D11Framework/D3D11ComputePipeline.hpp>

#include <memory>

namespace D3D11CoreLib {
namespace Processing {

class D3D11ThresholdProcessor {
public:
    D3D11ThresholdProcessor();
    ~D3D11ThresholdProcessor();

    D3D11ThresholdProcessor(const D3D11ThresholdProcessor&) = delete;
    D3D11ThresholdProcessor& operator=(const D3D11ThresholdProcessor&) = delete;
    D3D11ThresholdProcessor(D3D11ThresholdProcessor&&) noexcept;
    D3D11ThresholdProcessor& operator=(D3D11ThresholdProcessor&&) noexcept;

    void Initialize(D3D11ProcessingContext& context);

    void DispatchThreshold(
        ID3D11DeviceContext* deviceContext,
        D3D11Resource& src,
        D3D11Resource& dst,
        const ThresholdDesc& desc);

    void DispatchRangeThreshold(
        ID3D11DeviceContext* deviceContext,
        D3D11Resource& src,
        D3D11Resource& dst,
        const RangeThresholdDesc& desc);

    void DispatchConfidenceHeatmap(
        ID3D11DeviceContext* deviceContext,
        D3D11Resource& src,
        D3D11Resource& dst,
        const ConfidenceHeatmapDesc& desc);

    void DispatchClassColorMap(
        ID3D11DeviceContext* deviceContext,
        D3D11Resource& src,
        D3D11Resource& dst,
        const ClassColorMapDesc& desc);

    void DispatchMaskOverlay(
        ID3D11DeviceContext* deviceContext,
        D3D11Resource& mask,
        D3D11Resource& dst,
        const MaskOverlayDesc& desc);

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
};

} // namespace Processing
} // namespace D3D11CoreLib
