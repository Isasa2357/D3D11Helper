# D3D11Presentation

`D3D11Presentation` は、ウィンドウ表示・swapchain・backbuffer・render target 周辺を扱うモジュールです。

v1.1.0 では低レベル swapchain helper の移動だけでしたが、v1.3.0 では以下の高レベル wrapper を追加しています。

- `D3D11RenderTarget`
- `D3D11SwapChain`

これにより、アプリケーション側は RTV / DSV / viewport / resize まわりの定型処理をより短く書けます。

---

## Include

```cpp
#include <D3D11Helper/D3D11Presentation/D3D11Presentation.hpp>
```

個別 include:

```cpp
#include <D3D11Helper/D3D11Presentation/D3D11RenderTarget.hpp>
#include <D3D11Helper/D3D11Presentation/D3D11SwapChain.hpp>
#include <D3D11Helper/D3D11Presentation/D3D11SwapChainHelper.hpp>
```

---

## D3D11RenderTarget

`D3D11RenderTarget` は offscreen render target の color texture / RTV / optional SRV / optional depth-stencil texture / DSV をまとめる helper です。

```cpp
D3D11RenderTargetDesc desc = {};
desc.width = 1280;
desc.height = 720;
desc.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
desc.depthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
desc.createShaderResource = true;
desc.clearColor[0] = 0.05f;
desc.clearColor[1] = 0.07f;
desc.clearColor[2] = 0.10f;
desc.clearColor[3] = 1.0f;

auto target = CreateRenderTarget(*core, desc);

target.Clear(core->GetImmediateContext());
target.BindAndSetViewport(core->GetImmediateContext());
```

主な API:

```cpp
struct D3D11RenderTargetDesc {
    UINT width;
    UINT height;
    DXGI_FORMAT colorFormat;
    DXGI_FORMAT depthFormat;
    bool createShaderResource;
    UINT sampleCount;
    UINT sampleQuality;
    float clearColor[4];
    float clearDepth;
    UINT8 clearStencil;
};

class D3D11RenderTarget {
public:
    void Initialize(D3D11Core& core, const D3D11RenderTargetDesc& desc);
    void Reset() noexcept;

    UINT Width() const noexcept;
    UINT Height() const noexcept;
    DXGI_FORMAT ColorFormat() const noexcept;
    DXGI_FORMAT DepthFormat() const noexcept;

    D3D11Resource& ColorTexture() noexcept;
    D3D11Resource& DepthTexture() noexcept;

    ID3D11RenderTargetView* Rtv() const noexcept;
    ID3D11ShaderResourceView* Srv() const noexcept;
    ID3D11DepthStencilView* Dsv() const noexcept;

    D3D11_VIEWPORT Viewport(...) const noexcept;

    void Bind(ID3D11DeviceContext* context) const;
    void SetViewport(ID3D11DeviceContext* context) const;
    void BindAndSetViewport(ID3D11DeviceContext* context) const;

    void ClearColor(ID3D11DeviceContext* context) const;
    void ClearDepthStencil(ID3D11DeviceContext* context) const;
    void Clear(ID3D11DeviceContext* context) const;
};
```

### 想定用途

- offscreen rendering
- postprocess の入力 texture 生成
- shadow map ではない通常の color render target
- `D3D11Processing` へ渡す中間 texture
- CPU readback 前の render target

v1.3.0 では non-MSAA render target を対象にしています。MSAA render target と resolve helper は後続バージョンで分けて扱う想定です。

---

## D3D11SwapChain

`D3D11SwapChain` は `IDXGISwapChain3` と、その backbuffer RTV / optional depth-stencil target / viewport / resize をまとめる window presentation helper です。

```cpp
D3D11SwapChainDesc desc = {};
desc.hwnd = hwnd;
desc.width = 1280;
desc.height = 720;
desc.bufferCount = 2;
desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
desc.createDepthStencil = true;

auto swapChain = CreateSwapChain(*core, desc);

swapChain.Clear(core->GetImmediateContext());
swapChain.BindAndSetViewport(core->GetImmediateContext());

// draw...

swapChain.Present(1, 0);
```

主な API:

```cpp
struct D3D11SwapChainDesc {
    HWND hwnd;
    UINT width;
    UINT height;
    UINT bufferCount;
    DXGI_FORMAT format;
    bool createDepthStencil;
    DXGI_FORMAT depthFormat;
    float clearColor[4];
    float clearDepth;
    UINT8 clearStencil;
};

class D3D11SwapChain {
public:
    void Initialize(D3D11Core& core, const D3D11SwapChainDesc& desc);
    void Reset() noexcept;

    IDXGISwapChain3* Get() const noexcept;
    UINT Width() const noexcept;
    UINT Height() const noexcept;
    UINT BufferCount() const noexcept;
    DXGI_FORMAT Format() const noexcept;
    bool HasDepthStencil() const noexcept;

    UINT CurrentBackBufferIndex() const;

    D3D11Resource& CurrentBackBuffer();
    ID3D11RenderTargetView* CurrentRtv() const;
    ID3D11DepthStencilView* Dsv() const noexcept;

    D3D11_VIEWPORT Viewport(...) const noexcept;

    void BindCurrentBackBuffer(ID3D11DeviceContext* context) const;
    void SetViewport(ID3D11DeviceContext* context) const;
    void BindAndSetViewport(ID3D11DeviceContext* context) const;

    void ClearCurrentBackBuffer(ID3D11DeviceContext* context) const;
    void ClearDepthStencil(ID3D11DeviceContext* context) const;
    void Clear(ID3D11DeviceContext* context) const;

    void Present(UINT syncInterval = 1, UINT flags = 0);

    void Resize(UINT width, UINT height);
    void ResizeToClientRect(HWND hwnd = nullptr);
};
```

### Resize

Win32 の `WM_SIZE` では、最小化時を除いて `Resize()` または `ResizeToClientRect()` を呼びます。

```cpp
case WM_SIZE:
    if (swapChain && wp != SIZE_MINIMIZED) {
        swapChain.Resize(LOWORD(lp), HIWORD(lp));
    }
    return 0;
```

`D3D11SwapChain` は resize 時に RTV / DSV / size-dependent resource を内部で解放・再作成します。

---

## 低レベル swapchain helper

v1.0 からの低レベル API も残しています。

```cpp
ComPtr<IDXGISwapChain3> CreateSwapChainForHwnd(
    D3D11Core& core,
    HWND hwnd,
    UINT width,
    UINT height,
    UINT bufferCount = 2,
    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);

D3D11Resource GetSwapChainBackBuffer(IDXGISwapChain3* swapChain, UINT index);
```

これらは swapchain 作成と backbuffer 取得だけを行います。RTV 管理、depth-stencil、resize 対応を自分で持つ場合にはこの低レベル API を使えます。

新規コードでは、基本的に `D3D11SwapChain` を推奨します。

---

## Utility

```cpp
D3D11_VIEWPORT MakeViewport(UINT width, UINT height, ...);
void SetViewport(ID3D11DeviceContext* context, UINT width, UINT height, ...);
void UnbindRenderTargets(ID3D11DeviceContext* context, UINT renderTargetCount = 1);
```

`UnbindRenderTargets()` は resize 前や render target を SRV として読む前など、OM stage から RTV / DSV を外したい場合に使います。

---

## サンプル

v1.3.0 では以下を追加しています。

```text
sample/19_PresentationWindow
```

このサンプルでは、Win32 window と `D3D11SwapChain` を使い、resize 可能な最小 render loop で三角形を描画します。

---

## D3D11 flip-model swapchain の注意

`D3D11SwapChain` は `BufferCount()` として native swapchain 作成時の buffer count を保持しますが、内部で cache する backbuffer は `GetBuffer(0)` のみです。

D3D11 の flip-model swapchain では、環境によって `GetBuffer(1+)` が `DXGI_ERROR_INVALID_CALL` になることがあります。そのため、この wrapper は app-facing な render target として `GetBuffer(0)` を使い、runtime 内部の buffer rotation は DXGI に任せます。

---

## 互換パス

旧パスも v1.x では維持されます。

```cpp
#include <D3D11Helper/D3D11Framework/D3D11SwapChainHelper.hpp>
```

ただし、新規コードでは `D3D11Presentation/` を推奨します。
