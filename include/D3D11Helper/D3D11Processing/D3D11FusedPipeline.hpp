#pragma once
//
// D3D11FusedPipeline.hpp
// One-dispatch fused Processing passes.
//
#include <D3D11Helper/D3D11Processing/D3D11ProcessingContext.hpp>
#include <D3D11Helper/D3D11Processing/D3D11ProcessingShaderCache.hpp>
#include <D3D11Helper/D3D11Processing/D3D11TextureViews.hpp>
#include <D3D11Helper/D3D11Framework/D3D11ComputePipeline.hpp>

#include <memory>

namespace D3D11CoreLib {
namespace Processing {

class D3D11FusedProcessor {
public:
    D3D11FusedProcessor();
    ~D3D11FusedProcessor();

    D3D11FusedProcessor(const D3D11FusedProcessor&) = delete;
    D3D11FusedProcessor& operator=(const D3D11FusedProcessor&) = delete;
    D3D11FusedProcessor(D3D11FusedProcessor&&) noexcept;
    D3D11FusedProcessor& operator=(D3D11FusedProcessor&&) noexcept;

    void Initialize(D3D11ProcessingContext& context);

    void DispatchConvertResize(
        ID3D11DeviceContext* deviceContext,
        D3D11Resource& src,
        D3D11Resource& dst,
        const FusedConvertResizeDesc& desc);

    void DispatchConvertResizeView(
        ID3D11DeviceContext* deviceContext,
        D3D11ResourceView src,
        D3D11ResourceView dst,
        const FusedConvertResizeDesc& desc);

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
