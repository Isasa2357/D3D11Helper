# D3D11Helper v1.3.0 Migration Guide

## 概要

`v1.3.0` は backward-compatible な機能追加です。既存コードの必須変更はありません。

ただし、window rendering / resize / render target 管理を書いているコードでは、新しい `D3D11RenderTarget` と `D3D11SwapChain` へ移行すると定型処理を減らせます。

---

## 1. Include

従来:

```cpp
#include <D3D11Helper/D3D11Presentation/D3D11SwapChainHelper.hpp>
```

v1.3.0 推奨:

```cpp
#include <D3D11Helper/D3D11Presentation/D3D11Presentation.hpp>
```

または個別 include:

```cpp
#include <D3D11Helper/D3D11Presentation/D3D11RenderTarget.hpp>
#include <D3D11Helper/D3D11Presentation/D3D11SwapChain.hpp>
```

---

## 2. Offscreen render target への移行

従来は texture / RTV / SRV / DSV を個別に作る必要がありました。

v1.3.0 では次のようにまとめられます。

```cpp
D3D11RenderTargetDesc desc = {};
desc.width = width;
desc.height = height;
desc.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
desc.depthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
desc.createShaderResource = true;

auto target = CreateRenderTarget(*core, desc);

target.Clear(ctx);
target.BindAndSetViewport(ctx);
```

描画結果を shader input として使う場合は `target.Srv()` を使います。

---

## 3. Swapchain への移行

従来:

```cpp
auto swapChain = CreateSwapChainForHwnd(*core, hwnd, width, height);
auto backBuffer = GetSwapChainBackBuffer(swapChain.Get(), 0);
auto rtv = CreateTexture2DRtv(*core, backBuffer);

ID3D11RenderTargetView* rtvs[] = { rtv.Get() };
ctx->OMSetRenderTargets(1, rtvs, nullptr);
ctx->RSSetViewports(1, &viewport);
swapChain->Present(1, 0);
```

v1.3.0 推奨:

```cpp
D3D11SwapChainDesc desc = {};
desc.hwnd = hwnd;
desc.width = width;
desc.height = height;
desc.createDepthStencil = true;

auto swapChain = CreateSwapChain(*core, desc);

swapChain.Clear(ctx);
swapChain.BindAndSetViewport(ctx);
// draw...
swapChain.Present(1, 0);
```

---

## 4. Resize 対応

従来は resize 時に RTV / DSV / backbuffer を明示的に解放・再取得する必要がありました。

v1.3.0 では `D3D11SwapChain::Resize()` または `ResizeToClientRect()` を使います。

```cpp
case WM_SIZE:
    if (swapChain && wp != SIZE_MINIMIZED) {
        const UINT w = LOWORD(lp);
        const UINT h = HIWORD(lp);
        if (w > 0 && h > 0) {
            swapChain.Resize(w, h);
        }
    }
    return 0;
```

resize 後の RTV / DSV は `D3D11SwapChain` 内部で更新されます。外部で古い RTV / DSV を保持し続けないでください。

---

## 5. 既存の低レベル API を残す場合

既存の `CreateSwapChainForHwnd()` / `GetSwapChainBackBuffer()` を使い続けても構いません。

低レベル API が向いているケース:

- 独自の swapchain policy を持つ
- RTV / DSV の lifetime を完全に自前管理したい
- wrapper の resize 方針と合わない
- 既存コードを最小変更で維持したい

新規コードでは `D3D11SwapChain` を使う方が安全です。

---

## 6. D3D11 flip-model backbuffer の注意

`D3D11SwapChain` は `GetBuffer(0)` のみを cache します。

`BufferCount()` は native swapchain 作成時の requested buffer count を返しますが、`BackBuffer(1+)` を前提にしたコードは書かないでください。D3D11 flip-model swapchain では `GetBuffer(1+)` が失敗する環境があります。
