#pragma once
//
// D3D11Blur.hpp
// Separable GPU blur pass for RGBA-like textures.
//
#include <D3D11Helper/D3D11Processing/D3D11ProcessingContext.hpp>
#include <D3D11Helper/D3D11Processing/D3D11ProcessingShaderCache.hpp>
#include <D3D11Helper/D3D11Processing/D3D11TextureViews.hpp>
#include <D3D11Helper/D3D11Framework/D3D11ComputePipeline.hpp>

#include <memory>

namespace D3D11CoreLib {
namespace Processing {

class D3D11Blurrer {
public:
    static constexpr UINT MaxRadius = 16;

    D3D11Blurrer();
    ~D3D11Blurrer();

    D3D11Blurrer(const D3D11Blurrer&) = delete;
    D3D11Blurrer& operator=(const D3D11Blurrer&) = delete;
    D3D11Blurrer(D3D11Blurrer&&) noexcept;
    D3D11Blurrer& operator=(D3D11Blurrer&&) noexcept;

    void Initialize(D3D11ProcessingContext& context);

    void DispatchBlur(
        ID3D11DeviceContext* deviceContext,
        D3D11Resource& src,
        D3D11Resource& scratch,
        D3D11Resource& dst,
        const BlurDesc& desc);

    void DispatchBlurView(
        ID3D11DeviceContext* deviceContext,
        D3D11ResourceView src,
        D3D11ResourceView scratch,
        D3D11ResourceView dst,
        const BlurDesc& desc);

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
