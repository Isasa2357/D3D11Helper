#pragma once
//
// D3D11PyramidRegionBlur.hpp
// Region-masked fast blur built from D3D11PyramidBlur + original/blurred mask blend.
//
#include <D3D11Helper/D3D11Processing/D3D11PyramidBlur.hpp>
#include <D3D11Helper/D3D11Processing/D3D11ProcessingShaderCache.hpp>
#include <D3D11Helper/D3D11Processing/D3D11TextureViews.hpp>
#include <D3D11Helper/D3D11Framework/D3D11ComputePipeline.hpp>

#include <memory>

namespace D3D11CoreLib {
namespace Processing {

struct D3D11PyramidRegionBlurWorkspace {
    UINT sourceWidth = 0;
    UINT sourceHeight = 0;
    UINT levels = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

    D3D11PyramidBlurWorkspace blurWorkspace;
    D3D11Resource blurred;
};

class D3D11PyramidRegionBlur {
public:
    D3D11PyramidRegionBlur();
    ~D3D11PyramidRegionBlur();

    D3D11PyramidRegionBlur(const D3D11PyramidRegionBlur&) = delete;
    D3D11PyramidRegionBlur& operator=(const D3D11PyramidRegionBlur&) = delete;
    D3D11PyramidRegionBlur(D3D11PyramidRegionBlur&&) noexcept;
    D3D11PyramidRegionBlur& operator=(D3D11PyramidRegionBlur&&) noexcept;

    void Initialize(D3D11ProcessingContext& context);

    D3D11PyramidRegionBlurWorkspace CreateWorkspace(
        D3D11Core& core,
        UINT width,
        UINT height,
        DXGI_FORMAT format,
        UINT levels);

    D3D11Resource CreateOutputTexture(
        D3D11Core& core,
        UINT width,
        UINT height,
        DXGI_FORMAT format);

    void DispatchPyramidRegionBlur(
        ID3D11DeviceContext* deviceContext,
        D3D11Resource& src,
        D3D11PyramidRegionBlurWorkspace& workspace,
        D3D11Resource& dst,
        const PyramidRegionBlurDesc& desc);

private:
    struct Pipelines;

    void EnsureInitialized() const;
    void EnsurePipelines();

    D3D11ProcessingContext* m_context = nullptr;
    D3D11ProcessingShaderCache m_shaderCache;
    D3D11PyramidBlur m_pyramidBlur;
    std::unique_ptr<Pipelines> m_pipelines;
};

} // namespace Processing
} // namespace D3D11CoreLib
