#pragma once
//
// D3D11SwapChain.hpp - swap-chain back-buffer / resize convenience wrapper
//
// This belongs to the D3D11Presentation module.  The older
// D3D11SwapChainHelper.hpp remains the low-level creation helper; this wrapper
// owns the swap chain plus its size-dependent back-buffer RTVs and optional
// depth-stencil target.
//
#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Resource.hpp>
#include <D3D11Helper/D3D11Presentation/D3D11SwapChainHelper.hpp>

#include <vector>

namespace D3D11CoreLib {

struct D3D11SwapChainDesc {
    HWND hwnd = nullptr;

    UINT width = 0;
    UINT height = 0;
    UINT bufferCount = 2;

    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;

    bool createDepthStencil = false;
    DXGI_FORMAT depthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    float clearDepth = 1.0f;
    UINT8 clearStencil = 0;
};

class D3D11SwapChain {
public:
    D3D11SwapChain() = default;
    ~D3D11SwapChain() = default;

    D3D11SwapChain(const D3D11SwapChain&) = delete;
    D3D11SwapChain& operator=(const D3D11SwapChain&) = delete;

    D3D11SwapChain(D3D11SwapChain&&) noexcept = default;
    D3D11SwapChain& operator=(D3D11SwapChain&&) noexcept = default;

    void Initialize(D3D11Core& core, const D3D11SwapChainDesc& desc);
    void Reset() noexcept;

    bool IsValid() const noexcept { return m_swapChain != nullptr; }
    explicit operator bool() const noexcept { return IsValid(); }

    IDXGISwapChain3* Get() const noexcept { return m_swapChain.Get(); }
    const D3D11SwapChainDesc& Desc() const noexcept { return m_desc; }

    HWND Hwnd() const noexcept { return m_desc.hwnd; }
    UINT Width() const noexcept { return m_desc.width; }
    UINT Height() const noexcept { return m_desc.height; }
    // Native swap-chain buffer count requested at creation time.
    //
    // Note: For D3D11 flip-model swap chains, IDXGISwapChain::GetBuffer is
    // only used with index 0 by this wrapper.  The runtime rotates the
    // underlying buffers internally, while the RTV obtained from GetBuffer(0)
    // remains the app-facing render target.
    UINT BufferCount() const noexcept { return m_desc.bufferCount; }
    DXGI_FORMAT Format() const noexcept { return m_desc.format; }
    bool HasDepthStencil() const noexcept { return m_dsv != nullptr; }

    UINT CurrentBackBufferIndex() const;

    D3D11Resource& BackBuffer(UINT index);
    const D3D11Resource& BackBuffer(UINT index) const;

    D3D11Resource& CurrentBackBuffer();
    const D3D11Resource& CurrentBackBuffer() const;

    ID3D11RenderTargetView* Rtv(UINT index) const;
    ID3D11RenderTargetView* CurrentRtv() const;
    ID3D11DepthStencilView* Dsv() const noexcept { return m_dsv.Get(); }

    D3D11_VIEWPORT Viewport(
        FLOAT topLeftX = 0.0f,
        FLOAT topLeftY = 0.0f,
        FLOAT minDepth = 0.0f,
        FLOAT maxDepth = 1.0f) const noexcept;

    void BindCurrentBackBuffer(ID3D11DeviceContext* context) const;
    void SetViewport(ID3D11DeviceContext* context) const;
    void BindAndSetViewport(ID3D11DeviceContext* context) const;

    void ClearCurrentBackBuffer(ID3D11DeviceContext* context) const;
    void ClearDepthStencil(ID3D11DeviceContext* context) const;
    void Clear(ID3D11DeviceContext* context) const;

    void Present(UINT syncInterval = 1, UINT flags = 0);

    void Resize(UINT width, UINT height);
    void ResizeToClientRect(HWND hwnd = nullptr);

private:
    void ReleaseSizeDependentResources() noexcept;
    void CreateSizeDependentResources();
    void CreateBackBufferViews();
    void CreateDepthStencilTargetIfNeeded();
    void ValidateInitialized(const char* functionName) const;

    D3D11Core* m_core = nullptr;
    D3D11SwapChainDesc m_desc{};

    ComPtr<IDXGISwapChain3> m_swapChain;
    // D3D11 flip-model swap chains expose the app-facing back buffer through
    // GetBuffer(0).  Buffers with index > 0 are not cached here because some
    // D3D11/DXGI implementations reject GetBuffer(1+) with DXGI_ERROR_INVALID_CALL.
    std::vector<D3D11Resource> m_backBuffers;
    std::vector<ComPtr<ID3D11RenderTargetView>> m_rtvs;

    D3D11Resource m_depthTexture;
    ComPtr<ID3D11DepthStencilView> m_dsv;
};

D3D11SwapChain CreateSwapChain(
    D3D11Core& core,
    const D3D11SwapChainDesc& desc);

D3D11SwapChain CreateSwapChainForWindow(
    D3D11Core& core,
    HWND hwnd,
    UINT width,
    UINT height,
    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM,
    bool createDepthStencil = false,
    UINT bufferCount = 2);

} // namespace D3D11CoreLib
