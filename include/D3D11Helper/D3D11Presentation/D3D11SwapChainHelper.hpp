#pragma once
//
// D3D11SwapChainHelper.hpp
// ウィンドウ表示のための「作成ヘルパだけ」を提供する。
// Present ループ・RTV 運用はアプリ / サンプル側に置く方針。
//
// This header belongs to D3D11Presentation in the v1.1.0 architecture.
// The legacy path D3D11Framework/D3D11SwapChainHelper.hpp remains as a
// compatibility wrapper.
//
#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Resource.hpp>

namespace D3D11CoreLib {

// HWND と core から FLIP_DISCARD のスワップチェーンを作る。
// 戻り値は IDXGISwapChain3（GetCurrentBackBufferIndex が使える）。
ComPtr<IDXGISwapChain3> CreateSwapChainForHwnd(
    D3D11Core& core,
    HWND hwnd,
    UINT width, UINT height,
    UINT bufferCount = 2,
    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);

// index 番目のバックバッファを D3D11Resource として取得する。
D3D11Resource GetSwapChainBackBuffer(IDXGISwapChain3* swapChain, UINT index);

} // namespace D3D11CoreLib
