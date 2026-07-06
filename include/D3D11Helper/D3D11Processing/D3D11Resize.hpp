#pragma once
//
// D3D11Resize.hpp
// GPU RGBA-like resize pass.
//
#include <D3D11Helper/D3D11Processing/D3D11ProcessingContext.hpp>
#include <D3D11Helper/D3D11Processing/D3D11ProcessingShaderCache.hpp>
#include <D3D11Helper/D3D11Processing/D3D11TextureViews.hpp>
#include <D3D11Helper/D3D11Framework/D3D11ComputePipeline.hpp>

#include <memory>

namespace D3D11CoreLib {
namespace Processing {

class D3D11Resizer {
public:
    D3D11Resizer();
    ~D3D11Resizer();

    D3D11Resizer(const D3D11Resizer&) = delete;
    D3D11Resizer& operator=(const D3D11Resizer&) = delete;
    D3D11Resizer(D3D11Resizer&&) noexcept;
    D3D11Resizer& operator=(D3D11Resizer&&) noexcept;

    void Initialize(D3D11ProcessingContext& context);

    void DispatchResize(
        ID3D11DeviceContext* deviceContext,
        D3D11Resource& src,
        D3D11Resource& dst,
        const ResizeDesc& desc);

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
