#pragma once
//
// D3D11Composite.hpp
// GPU composite / blend pass for RGBA-like textures.
//
#include "D3D11ProcessingContext.hpp"
#include "D3D11ProcessingShaderCache.hpp"
#include "D3D11TextureViews.hpp"
#include "../D3D11Framework/D3D11ComputePipeline.hpp"

#include <memory>

namespace D3D11CoreLib {
namespace Processing {

class D3D11Compositor {
public:
    D3D11Compositor();
    ~D3D11Compositor();

    D3D11Compositor(const D3D11Compositor&) = delete;
    D3D11Compositor& operator=(const D3D11Compositor&) = delete;
    D3D11Compositor(D3D11Compositor&&) noexcept;
    D3D11Compositor& operator=(D3D11Compositor&&) noexcept;

    void Initialize(D3D11ProcessingContext& context);

    void DispatchComposite(
        ID3D11DeviceContext* deviceContext,
        D3D11Resource& base,
        D3D11Resource& overlay,
        D3D11Resource& dst,
        const CompositeDesc& desc);

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
