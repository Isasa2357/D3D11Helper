# D3D11Helper v1.3.0 Release Notes

## 概要

`v1.3.0` は **Presentation helper** の追加版です。

これまで `D3D11Presentation` は低レベルな swapchain 作成補助に近い位置づけでしたが、v1.3.0 では window rendering と offscreen rendering で頻出する RTV / DSV / viewport / resize / present の定型処理を扱う wrapper を追加しました。

---

## 追加機能

### D3D11RenderTarget

Offscreen render target をまとめる helper です。

- color texture
- render target view
- optional shader resource view
- optional depth/stencil texture
- depth/stencil view
- viewport
- clear
- bind

代表 API:

```cpp
D3D11RenderTargetDesc;
D3D11RenderTarget;
CreateRenderTarget();
MakeViewport();
SetViewport();
UnbindRenderTargets();
```

### D3D11SwapChain

Window swapchain をまとめる helper です。

- `IDXGISwapChain3`
- current backbuffer render target view
- optional depth/stencil target
- viewport
- clear
- bind
- present
- resize

代表 API:

```cpp
D3D11SwapChainDesc;
D3D11SwapChain;
CreateSwapChain();
CreateSwapChainForWindow();
```

### sample/19_PresentationWindow

Win32 window と `D3D11SwapChain` を使った最小 render loop サンプルを追加しました。

このサンプルでは以下を確認できます。

- window creation
- swapchain initialization
- resize handling
- render target clear
- viewport setup
- triangle drawing
- present

---

## 互換性

既存の低レベル API は残っています。

```cpp
CreateSwapChainForHwnd();
GetSwapChainBackBuffer();
```

既存コードの移行は必須ではありません。

ただし、新規コードや resize 対応 window では `D3D11SwapChain` を推奨します。

---

## 対応範囲

v1.3.0 の Presentation helper は次を対象にしています。

- Win32 `HWND`
- flip-model swapchain
- non-MSAA render target
- color RTV
- optional depth/stencil DSV
- viewport setup
- clear / bind / present / resize

---

## 非対応・非目標

以下は v1.3.0 では対象外です。

- full renderer / scene graph
- material system
- UI framework
- ImGui integration
- HDR swapchain policy
- MSAA render target / resolve helper
- multi-window render manager
- advanced frame pacing

---

## 注意: D3D11 flip-model swapchain

`D3D11SwapChain` は内部で `GetBuffer(0)` のみを app-facing render target として cache します。

`BufferCount > 1` でも、D3D11 flip-model swapchain では環境によって `GetBuffer(1+)` が `DXGI_ERROR_INVALID_CALL` を返すことがあります。そのため、runtime 内部の buffer rotation は DXGI に任せ、アプリ側は `GetBuffer(0)` を render target として扱う方針です。
