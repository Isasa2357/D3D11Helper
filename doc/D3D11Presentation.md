# D3D11Presentation

`D3D11Presentation` は、ウィンドウ表示と swapchain 周辺を扱うモジュールです。

## Include

```cpp
#include <D3D11Helper/D3D11Presentation/D3D11Presentation.hpp>
```

## 現在の機能

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

## 今後の拡張予定

後続バージョンでは、次のような高レベル wrapper を追加する予定です。

- `D3D11SwapChain`
- `D3D11RenderTarget`
- `D3D11BackBuffer`
- `D3D11SwapChainRenderer`
- resize handling
- begin/end frame
- viewport / scissor helper

## 互換パス

旧パスも v1.x では維持されます。

```cpp
#include <D3D11Helper/D3D11Framework/D3D11SwapChainHelper.hpp>
```
