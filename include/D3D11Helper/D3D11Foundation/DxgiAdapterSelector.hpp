#pragma once
//
// DxgiAdapterSelector.hpp - DXGI Factory 生成とアダプタ選択
//

#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>

namespace D3D11CoreLib {

class DxgiAdapterSelector {
public:
    // DXGI Factory を生成。enableDebug 時は DXGI_CREATE_FACTORY_DEBUG を試み、
    // デバッグ層が無ければ自動で非デバッグ再試行する。
    static ComPtr<IDXGIFactory6> CreateFactory(bool enableDebug);

    // ハードウェアアダプタを選択（D3D11 Device を作成できる最初のもの）。
    // 見つからず allowWarp=true なら WARP を返す。両方無ければ nullptr。
    static ComPtr<IDXGIAdapter1> SelectHardwareAdapter(
        IDXGIFactory6* factory, bool preferHighPerformance, bool allowWarp);

    // LUID 指定。見つからない / D3D11 非対応なら例外。
    static ComPtr<IDXGIAdapter1> SelectAdapterByLuid(IDXGIFactory6* factory, LUID luid);
};

} // namespace D3D11CoreLib
