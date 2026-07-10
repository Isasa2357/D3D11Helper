#pragma once
//
// D3D11PyramidProcessor.hpp
// 2x downsample / 2x upsample primitives for RGBA-like textures.
//
#include <D3D11Helper/D3D11Processing/D3D11ProcessingContext.hpp>
#include <D3D11Helper/D3D11Processing/D3D11ProcessingShaderCache.hpp>
#include <D3D11Helper/D3D11Processing/D3D11TextureViews.hpp>
#include <D3D11Helper/D3D11Framework/D3D11ComputePipeline.hpp>

#include <memory>

namespace D3D11CoreLib {
namespace Processing {

class D3D11PyramidProcessor {
public:
    D3D11PyramidProcessor();
    ~D3D11PyramidProcessor();

    D3D11PyramidProcessor(const D3D11PyramidProcessor&) = delete;
    D3D11PyramidProcessor& operator=(const D3D11PyramidProcessor&) = delete;
    D3D11PyramidProcessor(D3D11PyramidProcessor&&) noexcept;
    D3D11PyramidProcessor& operator=(D3D11PyramidProcessor&&) noexcept;

    void Initialize(D3D11ProcessingContext& context);

    void DispatchDownsample2x(
        ID3D11DeviceContext* deviceContext,
        D3D11Resource& src,
        D3D11Resource& dst,
        const PyramidDownsampleDesc& desc);

    void DispatchDownsample2xView(
        ID3D11DeviceContext* deviceContext,
        D3D11ResourceView src,
        D3D11ResourceView dst,
        const PyramidDownsampleDesc& desc);

    void DispatchUpsample2x(
        ID3D11DeviceContext* deviceContext,
        D3D11Resource& src,
        D3D11Resource& dst,
        const PyramidUpsampleDesc& desc);

    void DispatchUpsample2xView(
        ID3D11DeviceContext* deviceContext,
        D3D11ResourceView src,
        D3D11ResourceView dst,
        const PyramidUpsampleDesc& desc);

    D3D11Resource CreateOutputTexture(
        D3D11Core& core,
        UINT width,
        UINT height,
        DXGI_FORMAT format);

    D3D11Resource CreateDownsampledTexture(
        D3D11Core& core,
        UINT srcWidth,
        UINT srcHeight,
        DXGI_FORMAT format);

    D3D11Resource CreateUpsampledTexture(
        D3D11Core& core,
        UINT srcWidth,
        UINT srcHeight,
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
