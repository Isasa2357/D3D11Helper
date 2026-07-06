#pragma once
//
// D3D11RegionEffect.hpp
// Single-pass GPU region / mask effects for RGBA-like textures.
//
#include <D3D11Helper/D3D11Processing/D3D11ProcessingContext.hpp>
#include <D3D11Helper/D3D11Processing/D3D11ProcessingShaderCache.hpp>
#include <D3D11Helper/D3D11Processing/D3D11TextureViews.hpp>
#include <D3D11Helper/D3D11Framework/D3D11ComputePipeline.hpp>

#include <memory>

namespace D3D11CoreLib {
namespace Processing {

class D3D11RegionEffect {
public:
    D3D11RegionEffect();
    ~D3D11RegionEffect();

    D3D11RegionEffect(const D3D11RegionEffect&) = delete;
    D3D11RegionEffect& operator=(const D3D11RegionEffect&) = delete;
    D3D11RegionEffect(D3D11RegionEffect&&) noexcept;
    D3D11RegionEffect& operator=(D3D11RegionEffect&&) noexcept;

    void Initialize(D3D11ProcessingContext& context);

    void DispatchRegionEffect(
        ID3D11DeviceContext* deviceContext,
        D3D11Resource& src,
        D3D11Resource& dst,
        const RegionEffectDesc& desc);

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
