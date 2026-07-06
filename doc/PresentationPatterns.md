# Presentation Patterns

`D3D11Presentation` の代表的な使い方を示します。

---

## 1. Offscreen render target

```cpp
D3D11RenderTargetDesc desc = {};
desc.width = 1280;
desc.height = 720;
desc.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
desc.depthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
desc.createShaderResource = true;

auto target = CreateRenderTarget(*core, desc);

auto* ctx = core->GetImmediateContext();
target.Clear(ctx);
target.BindAndSetViewport(ctx);

// draw...

ID3D11ShaderResourceView* srv = target.Srv();
```

`createShaderResource = true` にしておくと、描画結果を後段の shader / processing に渡しやすくなります。

---

## 2. Window swapchain render loop

```cpp
D3D11SwapChainDesc desc = {};
desc.hwnd = hwnd;
desc.width = width;
desc.height = height;
desc.bufferCount = 2;
desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
desc.createDepthStencil = true;

auto swapChain = CreateSwapChain(*core, desc);

auto* ctx = core->GetImmediateContext();

while (running) {
    swapChain.Clear(ctx);
    swapChain.BindAndSetViewport(ctx);

    // draw...

    swapChain.Present(1, 0);
}
```

`BindAndSetViewport()` により、current RTV / optional DSV / viewport をまとめて設定できます。

---

## 3. WM_SIZE で resize

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

または、client rect を内部で読む場合:

```cpp
swapChain.ResizeToClientRect();
```

resize 時には RTV / DSV が作り直されるため、古い RTV / DSV を外部で保持し続けないでください。

---

## 4. Resize 前に OM binding を外す

自前で低レベル resize を行う場合や render target を SRV として読む前には、OM stage の binding を外します。

```cpp
UnbindRenderTargets(core->GetImmediateContext());
```

`D3D11SwapChain` は内部の resize 処理で size-dependent resources を解放・再作成します。

---

## 5. 低レベル helper を使う場合

RTV / DSV / resize を自前で管理したい場合は、従来の低レベル API も使えます。

```cpp
auto swapChain = CreateSwapChainForHwnd(*core, hwnd, width, height);
auto backBuffer = GetSwapChainBackBuffer(swapChain.Get(), 0);
auto rtv = CreateTexture2DRtv(*core, backBuffer);
```

新規コードでは、基本的に `D3D11SwapChain` の使用を推奨します。
