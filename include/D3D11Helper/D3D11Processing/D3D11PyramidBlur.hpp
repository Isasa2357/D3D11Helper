#pragma once
//
// D3D11PyramidBlur.hpp
// Fast approximate blur using downsample -> low-res blur -> upsample.
//
#include <D3D11Helper/D3D11Processing/D3D11Blur.hpp>
#include <D3D11Helper/D3D11Processing/D3D11PyramidProcessor.hpp>

#include <vector>

namespace D3D11CoreLib {
namespace Processing {

struct D3D11PyramidBlurWorkspace {
    UINT sourceWidth = 0;
    UINT sourceHeight = 0;
    UINT levels = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

    std::vector<D3D11Resource> downTextures;
    D3D11Resource blurScratch;
    D3D11Resource blurredLow;
    std::vector<D3D11Resource> upTextures;
};

class D3D11PyramidBlur {
public:
    static constexpr UINT MaxLevels = 6;

    D3D11PyramidBlur();
    ~D3D11PyramidBlur();

    D3D11PyramidBlur(const D3D11PyramidBlur&) = delete;
    D3D11PyramidBlur& operator=(const D3D11PyramidBlur&) = delete;
    D3D11PyramidBlur(D3D11PyramidBlur&&) noexcept;
    D3D11PyramidBlur& operator=(D3D11PyramidBlur&&) noexcept;

    void Initialize(D3D11ProcessingContext& context);

    D3D11PyramidBlurWorkspace CreateWorkspace(
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

    void DispatchPyramidBlur(
        ID3D11DeviceContext* deviceContext,
        D3D11Resource& src,
        D3D11PyramidBlurWorkspace& workspace,
        D3D11Resource& dst,
        const PyramidBlurDesc& desc);

private:
    void EnsureInitialized() const;

    D3D11ProcessingContext* m_context = nullptr;
    D3D11PyramidProcessor m_pyramid;
    D3D11Blurrer m_blurrer;
};

} // namespace Processing
} // namespace D3D11CoreLib
