#pragma once
//
// D3D11RegionBlur.hpp
// Region-masked blur built from D3D11Blurrer + original/blurred mask blend.
//
#include <D3D11Helper/D3D11Processing/D3D11Blur.hpp>
#include <D3D11Helper/D3D11Processing/D3D11ProcessingContext.hpp>
#include <D3D11Helper/D3D11Processing/D3D11ProcessingShaderCache.hpp>
#include <D3D11Helper/D3D11Processing/D3D11TextureViews.hpp>
#include <D3D11Helper/D3D11Framework/D3D11ComputePipeline.hpp>

#include <memory>

namespace D3D11CoreLib {
namespace Processing {

class D3D11RegionBlur {
public:
    D3D11RegionBlur();
    ~D3D11RegionBlur();

    D3D11RegionBlur(const D3D11RegionBlur&) = delete;
    D3D11RegionBlur& operator=(const D3D11RegionBlur&) = delete;
    D3D11RegionBlur(D3D11RegionBlur&&) noexcept;
    D3D11RegionBlur& operator=(D3D11RegionBlur&&) noexcept;

    void Initialize(D3D11ProcessingContext& context);

    // Runs blur into blurred, then blends original src and blurred according to RegionBlurDesc.
    // blurScratch and blurred must be different SRV+UAV RGBA-like textures.
    void DispatchRegionBlur(
        ID3D11DeviceContext* deviceContext,
        D3D11Resource& src,
        D3D11Resource& blurScratch,
        D3D11Resource& blurred,
        D3D11Resource& dst,
        const RegionBlurDesc& desc);

    void DispatchRegionBlurView(
        ID3D11DeviceContext* deviceContext,
        D3D11ResourceView src,
        D3D11ResourceView blurScratch,
        D3D11ResourceView blurred,
        D3D11ResourceView dst,
        const RegionBlurDesc& desc);

    D3D11Resource CreateOutputTexture(
        D3D11Core& core,
        UINT width,
        UINT height,
        DXGI_FORMAT format);

    D3D11Resource CreateScratchTexture(
        D3D11Core& core,
        UINT width,
        UINT height,
        DXGI_FORMAT format);

    D3D11Resource CreateBlurredTexture(
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
    D3D11Blurrer m_blurrer;
    std::unique_ptr<Pipelines> m_pipelines;
};

} // namespace Processing
} // namespace D3D11CoreLib
