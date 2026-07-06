#pragma once
//
// D3D11RenderTarget.hpp - render target / depth target convenience wrapper
//
// This belongs to the D3D11Presentation module.  It is intentionally a thin
// helper over the existing D3D11Gpu resource/view creation functions.
//
#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Resource.hpp>

namespace D3D11CoreLib {

struct D3D11RenderTargetDesc {
    UINT width = 0;
    UINT height = 0;

    DXGI_FORMAT colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

    // Set to DXGI_FORMAT_UNKNOWN to disable depth/stencil creation.
    DXGI_FORMAT depthFormat = DXGI_FORMAT_UNKNOWN;

    // Create an SRV for the color texture in addition to the RTV.
    bool createShaderResource = true;

    // v1.3.0 step 1 intentionally supports only non-MSAA render targets.
    UINT sampleCount = 1;
    UINT sampleQuality = 0;

    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    float clearDepth = 1.0f;
    UINT8 clearStencil = 0;
};

D3D11_VIEWPORT MakeViewport(
    UINT width,
    UINT height,
    FLOAT topLeftX = 0.0f,
    FLOAT topLeftY = 0.0f,
    FLOAT minDepth = 0.0f,
    FLOAT maxDepth = 1.0f) noexcept;

void SetViewport(
    ID3D11DeviceContext* context,
    UINT width,
    UINT height,
    FLOAT topLeftX = 0.0f,
    FLOAT topLeftY = 0.0f,
    FLOAT minDepth = 0.0f,
    FLOAT maxDepth = 1.0f);

// Clears render target and depth-stencil bindings from OM stage.
// renderTargetCount may be 0 to unbind only the DSV.
void UnbindRenderTargets(
    ID3D11DeviceContext* context,
    UINT renderTargetCount = 1);

class D3D11RenderTarget {
public:
    D3D11RenderTarget() = default;
    ~D3D11RenderTarget() = default;

    D3D11RenderTarget(const D3D11RenderTarget&) = delete;
    D3D11RenderTarget& operator=(const D3D11RenderTarget&) = delete;

    D3D11RenderTarget(D3D11RenderTarget&&) noexcept = default;
    D3D11RenderTarget& operator=(D3D11RenderTarget&&) noexcept = default;

    void Initialize(D3D11Core& core, const D3D11RenderTargetDesc& desc);
    void Reset() noexcept;

    bool IsValid() const noexcept { return m_rtv != nullptr; }
    explicit operator bool() const noexcept { return IsValid(); }

    const D3D11RenderTargetDesc& Desc() const noexcept { return m_desc; }

    UINT Width() const noexcept { return m_desc.width; }
    UINT Height() const noexcept { return m_desc.height; }
    DXGI_FORMAT ColorFormat() const noexcept { return m_desc.colorFormat; }
    DXGI_FORMAT DepthFormat() const noexcept { return m_desc.depthFormat; }

    D3D11Resource& ColorTexture() noexcept { return m_colorTexture; }
    const D3D11Resource& ColorTexture() const noexcept { return m_colorTexture; }

    D3D11Resource& DepthTexture() noexcept { return m_depthTexture; }
    const D3D11Resource& DepthTexture() const noexcept { return m_depthTexture; }

    ID3D11RenderTargetView* Rtv() const noexcept { return m_rtv.Get(); }
    ID3D11ShaderResourceView* Srv() const noexcept { return m_srv.Get(); }
    ID3D11DepthStencilView* Dsv() const noexcept { return m_dsv.Get(); }

    D3D11_VIEWPORT Viewport(
        FLOAT topLeftX = 0.0f,
        FLOAT topLeftY = 0.0f,
        FLOAT minDepth = 0.0f,
        FLOAT maxDepth = 1.0f) const noexcept;

    void Bind(ID3D11DeviceContext* context) const;
    void SetViewport(ID3D11DeviceContext* context) const;
    void BindAndSetViewport(ID3D11DeviceContext* context) const;

    void ClearColor(ID3D11DeviceContext* context) const;
    void ClearDepthStencil(ID3D11DeviceContext* context) const;
    void Clear(ID3D11DeviceContext* context) const;

private:
    D3D11RenderTargetDesc m_desc{};

    D3D11Resource m_colorTexture;
    D3D11Resource m_depthTexture;

    ComPtr<ID3D11RenderTargetView> m_rtv;
    ComPtr<ID3D11ShaderResourceView> m_srv;
    ComPtr<ID3D11DepthStencilView> m_dsv;
};

D3D11RenderTarget CreateRenderTarget(
    D3D11Core& core,
    const D3D11RenderTargetDesc& desc);

D3D11RenderTarget CreateRenderTarget(
    D3D11Core& core,
    UINT width,
    UINT height,
    DXGI_FORMAT colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
    bool createDepth = false,
    DXGI_FORMAT depthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT,
    bool createShaderResource = true);

} // namespace D3D11CoreLib
