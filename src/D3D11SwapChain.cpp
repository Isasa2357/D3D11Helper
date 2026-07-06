//
// D3D11SwapChain.cpp
//
#include <D3D11Helper/D3D11Presentation/D3D11SwapChain.hpp>
#include <D3D11Helper/D3D11Presentation/D3D11RenderTarget.hpp>
#include <D3D11Helper/D3D11Foundation/D3D11FormatUtil.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Helpers.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

namespace D3D11CoreLib {
namespace {

void ThrowIfNull(ID3D11DeviceContext* context, const char* name) {
    if (!context) {
        throw std::runtime_error(std::string(name) + ": null context");
    }
}

bool HasStencilComponent(DXGI_FORMAT format) noexcept {
    switch (format) {
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        return true;
    default:
        return false;
    }
}

D3D11SwapChainDesc NormalizeAndValidateDesc(const D3D11SwapChainDesc& input) {
    D3D11SwapChainDesc desc = input;

    if (!desc.hwnd) {
        throw std::runtime_error("D3D11SwapChain: null HWND");
    }
    if (desc.width == 0 || desc.height == 0) {
        throw std::runtime_error("D3D11SwapChain: width and height must be > 0");
    }
    if (desc.bufferCount < 2) {
        desc.bufferCount = 2;
    }
    if (desc.format == DXGI_FORMAT_UNKNOWN) {
        throw std::runtime_error("D3D11SwapChain: format must not be UNKNOWN");
    }
    if (FormatUtil::IsDepthFormat(desc.format)) {
        throw std::runtime_error("D3D11SwapChain: color format must not be a depth format");
    }
    if (FormatUtil::IsBlockCompressedFormat(desc.format) ||
        FormatUtil::IsPlanarFormat(desc.format)) {
        throw std::runtime_error("D3D11SwapChain: color format must be a single-plane render-target format");
    }
    if (desc.createDepthStencil) {
        if (desc.depthFormat == DXGI_FORMAT_UNKNOWN ||
            !FormatUtil::IsDepthFormat(desc.depthFormat)) {
            throw std::runtime_error("D3D11SwapChain: depthFormat must be a depth/stencil format");
        }
    }

    return desc;
}

RECT GetClientRectChecked(HWND hwnd, const char* functionName) {
    if (!hwnd) {
        throw std::runtime_error(std::string(functionName) + ": null HWND");
    }

    RECT rc{};
    if (!::GetClientRect(hwnd, &rc)) {
        throw std::runtime_error(std::string(functionName) + ": GetClientRect failed");
    }
    return rc;
}

} // unnamed namespace

void D3D11SwapChain::Initialize(D3D11Core& core, const D3D11SwapChainDesc& desc) {
    Reset();

    m_core = &core;
    m_desc = NormalizeAndValidateDesc(desc);

    m_swapChain = CreateSwapChainForHwnd(
        core,
        m_desc.hwnd,
        m_desc.width,
        m_desc.height,
        m_desc.bufferCount,
        m_desc.format);

    CreateSizeDependentResources();
}

void D3D11SwapChain::Reset() noexcept {
    ReleaseSizeDependentResources();
    m_swapChain.Reset();
    m_core = nullptr;
    m_desc = D3D11SwapChainDesc{};
}

UINT D3D11SwapChain::CurrentBackBufferIndex() const {
    ValidateInitialized("D3D11SwapChain::CurrentBackBufferIndex");
    return m_swapChain->GetCurrentBackBufferIndex();
}

D3D11Resource& D3D11SwapChain::BackBuffer(UINT index) {
    if (index >= m_backBuffers.size()) {
        throw std::runtime_error("D3D11SwapChain::BackBuffer: index out of range");
    }
    return m_backBuffers[index];
}

const D3D11Resource& D3D11SwapChain::BackBuffer(UINT index) const {
    if (index >= m_backBuffers.size()) {
        throw std::runtime_error("D3D11SwapChain::BackBuffer: index out of range");
    }
    return m_backBuffers[index];
}

D3D11Resource& D3D11SwapChain::CurrentBackBuffer() {
    // D3D11 flip-model swap chains expose the app-facing back buffer via index 0.
    // GetCurrentBackBufferIndex() is useful for diagnostics, but unlike D3D12 it
    // should not be used to index app-created RTVs in this wrapper.
    return BackBuffer(0);
}

const D3D11Resource& D3D11SwapChain::CurrentBackBuffer() const {
    return BackBuffer(0);
}

ID3D11RenderTargetView* D3D11SwapChain::Rtv(UINT index) const {
    if (index >= m_rtvs.size()) {
        throw std::runtime_error("D3D11SwapChain::Rtv: index out of range");
    }
    return m_rtvs[index].Get();
}

ID3D11RenderTargetView* D3D11SwapChain::CurrentRtv() const {
    // See CurrentBackBuffer(): D3D11 uses the GetBuffer(0) RTV for rendering.
    return Rtv(0);
}

D3D11_VIEWPORT D3D11SwapChain::Viewport(FLOAT topLeftX,
                                        FLOAT topLeftY,
                                        FLOAT minDepth,
                                        FLOAT maxDepth) const noexcept {
    return MakeViewport(m_desc.width, m_desc.height, topLeftX, topLeftY, minDepth, maxDepth);
}

void D3D11SwapChain::BindCurrentBackBuffer(ID3D11DeviceContext* context) const {
    ThrowIfNull(context, "D3D11SwapChain::BindCurrentBackBuffer");
    ValidateInitialized("D3D11SwapChain::BindCurrentBackBuffer");

    ID3D11RenderTargetView* rtv = CurrentRtv();
    context->OMSetRenderTargets(1, &rtv, m_dsv.Get());
}

void D3D11SwapChain::SetViewport(ID3D11DeviceContext* context) const {
    ThrowIfNull(context, "D3D11SwapChain::SetViewport");
    ValidateInitialized("D3D11SwapChain::SetViewport");

    const D3D11_VIEWPORT vp = Viewport();
    context->RSSetViewports(1, &vp);
}

void D3D11SwapChain::BindAndSetViewport(ID3D11DeviceContext* context) const {
    BindCurrentBackBuffer(context);
    SetViewport(context);
}

void D3D11SwapChain::ClearCurrentBackBuffer(ID3D11DeviceContext* context) const {
    ThrowIfNull(context, "D3D11SwapChain::ClearCurrentBackBuffer");
    ValidateInitialized("D3D11SwapChain::ClearCurrentBackBuffer");

    context->ClearRenderTargetView(CurrentRtv(), m_desc.clearColor);
}

void D3D11SwapChain::ClearDepthStencil(ID3D11DeviceContext* context) const {
    ThrowIfNull(context, "D3D11SwapChain::ClearDepthStencil");
    ValidateInitialized("D3D11SwapChain::ClearDepthStencil");

    if (!m_dsv) {
        return;
    }

    UINT clearFlags = D3D11_CLEAR_DEPTH;
    if (HasStencilComponent(m_desc.depthFormat)) {
        clearFlags |= D3D11_CLEAR_STENCIL;
    }
    context->ClearDepthStencilView(m_dsv.Get(), clearFlags, m_desc.clearDepth, m_desc.clearStencil);
}

void D3D11SwapChain::Clear(ID3D11DeviceContext* context) const {
    ClearCurrentBackBuffer(context);
    ClearDepthStencil(context);
}

void D3D11SwapChain::Present(UINT syncInterval, UINT flags) {
    ValidateInitialized("D3D11SwapChain::Present");
    D3D11CORE_THROW_IF_FAILED(m_swapChain->Present(syncInterval, flags));
}

void D3D11SwapChain::Resize(UINT width, UINT height) {
    ValidateInitialized("D3D11SwapChain::Resize");

    if (width == 0 || height == 0) {
        throw std::runtime_error("D3D11SwapChain::Resize: width and height must be > 0");
    }

    ID3D11DeviceContext* context = m_core->GetImmediateContext();
    if (context) {
        UnbindRenderTargets(context, 1);
        context->Flush();
    }

    ReleaseSizeDependentResources();

    D3D11CORE_THROW_IF_FAILED(m_swapChain->ResizeBuffers(
        m_desc.bufferCount,
        width,
        height,
        m_desc.format,
        0));

    m_desc.width = width;
    m_desc.height = height;

    CreateSizeDependentResources();
}

void D3D11SwapChain::ResizeToClientRect(HWND hwnd) {
    ValidateInitialized("D3D11SwapChain::ResizeToClientRect");

    HWND targetHwnd = hwnd ? hwnd : m_desc.hwnd;
    const RECT rc = GetClientRectChecked(targetHwnd, "D3D11SwapChain::ResizeToClientRect");

    const UINT width = static_cast<UINT>(std::max<LONG>(0, rc.right - rc.left));
    const UINT height = static_cast<UINT>(std::max<LONG>(0, rc.bottom - rc.top));

    if (width == 0 || height == 0) {
        throw std::runtime_error("D3D11SwapChain::ResizeToClientRect: client area is empty");
    }

    Resize(width, height);
}

void D3D11SwapChain::ReleaseSizeDependentResources() noexcept {
    m_dsv.Reset();
    m_depthTexture = D3D11Resource{};
    m_rtvs.clear();
    m_backBuffers.clear();
}

void D3D11SwapChain::CreateSizeDependentResources() {
    CreateBackBufferViews();
    CreateDepthStencilTargetIfNeeded();
}

void D3D11SwapChain::CreateBackBufferViews() {
    ValidateInitialized("D3D11SwapChain::CreateBackBufferViews");

    m_backBuffers.clear();
    m_rtvs.clear();

    // In D3D11 flip-model swap chains, the application-facing render target is
    // obtained through GetBuffer(0).  Querying GetBuffer(1+) may fail with
    // DXGI_ERROR_INVALID_CALL even when BufferCount is 2 or 3.  D3D12-style
    // per-back-buffer RTV arrays are therefore intentionally avoided here.
    D3D11Resource backBuffer = GetSwapChainBackBuffer(m_swapChain.Get(), 0);
    ComPtr<ID3D11RenderTargetView> rtv = CreateTexture2DRtv(*m_core, backBuffer, m_desc.format);

    m_backBuffers.push_back(std::move(backBuffer));
    m_rtvs.push_back(std::move(rtv));
}

void D3D11SwapChain::CreateDepthStencilTargetIfNeeded() {
    if (!m_desc.createDepthStencil) {
        return;
    }

    m_depthTexture = CreateTexture2D(
        *m_core,
        m_desc.width,
        m_desc.height,
        m_desc.depthFormat,
        D3D11_BIND_DEPTH_STENCIL,
        D3D11_USAGE_DEFAULT,
        0,
        1,
        1);
    m_dsv = CreateTexture2DDsv(*m_core, m_depthTexture, m_desc.depthFormat);
}

void D3D11SwapChain::ValidateInitialized(const char* functionName) const {
    if (!m_core || !m_swapChain) {
        throw std::runtime_error(std::string(functionName) + ": swap chain is not initialized");
    }
}

D3D11SwapChain CreateSwapChain(D3D11Core& core, const D3D11SwapChainDesc& desc) {
    D3D11SwapChain swapChain;
    swapChain.Initialize(core, desc);
    return swapChain;
}

D3D11SwapChain CreateSwapChainForWindow(D3D11Core& core,
                                        HWND hwnd,
                                        UINT width,
                                        UINT height,
                                        DXGI_FORMAT format,
                                        bool createDepthStencil,
                                        UINT bufferCount) {
    D3D11SwapChainDesc desc = {};
    desc.hwnd = hwnd;
    desc.width = width;
    desc.height = height;
    desc.format = format;
    desc.createDepthStencil = createDepthStencil;
    desc.bufferCount = bufferCount;
    return CreateSwapChain(core, desc);
}

} // namespace D3D11CoreLib
