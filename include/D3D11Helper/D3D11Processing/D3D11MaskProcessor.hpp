#pragma once
//
// D3D11MaskProcessor.hpp
// GPU mask application, mask blending, inversion, and mask combination passes.
//
#include <D3D11Helper/D3D11Processing/D3D11ProcessingContext.hpp>
#include <D3D11Helper/D3D11Processing/D3D11ProcessingShaderCache.hpp>
#include <D3D11Helper/D3D11Processing/D3D11TextureViews.hpp>
#include <D3D11Helper/D3D11Framework/D3D11ComputePipeline.hpp>

#include <memory>

namespace D3D11CoreLib {
namespace Processing {

class D3D11MaskProcessor {
public:
    D3D11MaskProcessor();
    ~D3D11MaskProcessor();

    D3D11MaskProcessor(const D3D11MaskProcessor&) = delete;
    D3D11MaskProcessor& operator=(const D3D11MaskProcessor&) = delete;
    D3D11MaskProcessor(D3D11MaskProcessor&&) noexcept;
    D3D11MaskProcessor& operator=(D3D11MaskProcessor&&) noexcept;

    void Initialize(D3D11ProcessingContext& context);

    void DispatchApplyMask(
        ID3D11DeviceContext* deviceContext,
        D3D11Resource& src,
        D3D11Resource& mask,
        D3D11Resource& dst,
        const MaskApplyDesc& desc);

    void DispatchApplyMaskView(
        ID3D11DeviceContext* deviceContext,
        D3D11ResourceView src,
        D3D11ResourceView mask,
        D3D11ResourceView dst,
        const MaskApplyDesc& desc);

    void DispatchBlendByMask(
        ID3D11DeviceContext* deviceContext,
        D3D11Resource& base,
        D3D11Resource& overlay,
        D3D11Resource& mask,
        D3D11Resource& dst,
        const MaskBlendDesc& desc);

    void DispatchBlendByMaskView(
        ID3D11DeviceContext* deviceContext,
        D3D11ResourceView base,
        D3D11ResourceView overlay,
        D3D11ResourceView mask,
        D3D11ResourceView dst,
        const MaskBlendDesc& desc);

    void DispatchCombineMasks(
        ID3D11DeviceContext* deviceContext,
        D3D11Resource& maskA,
        D3D11Resource& maskB,
        D3D11Resource& dst,
        const MaskCombineDesc& desc);

    void DispatchCombineMasksView(
        ID3D11DeviceContext* deviceContext,
        D3D11ResourceView maskA,
        D3D11ResourceView maskB,
        D3D11ResourceView dst,
        const MaskCombineDesc& desc);

    void DispatchInvertMask(
        ID3D11DeviceContext* deviceContext,
        D3D11Resource& mask,
        D3D11Resource& dst,
        const MaskInvertDesc& desc);

    void DispatchInvertMaskView(
        ID3D11DeviceContext* deviceContext,
        D3D11ResourceView mask,
        D3D11ResourceView dst,
        const MaskInvertDesc& desc);

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
