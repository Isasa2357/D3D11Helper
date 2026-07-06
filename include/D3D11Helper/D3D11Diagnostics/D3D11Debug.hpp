#pragma once
//
// D3D11Debug.hpp - Debug Layer / InfoQueue
//
// D3D12 と異なり、D3D11 では Device 作成時に D3D11_CREATE_DEVICE_DEBUG フラグで
// デバッグ層を有効化する。Device 作成後に InfoQueue を設定する。
//
// This header belongs to D3D11Diagnostics in the v1.1.0 architecture.
// The legacy path D3D11Core/D3D11Debug.hpp remains as a compatibility wrapper.
//
#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>

#include <cstring>

namespace D3D11CoreLib {
namespace D3D11Debug {

// InfoQueue の break / フィルタ設定。Device 作成後に呼ぶ。
void SetupInfoQueue(ID3D11Device* device,
                    bool breakOnError, bool breakOnCorruption, bool breakOnWarning);

// デバッグ時にライブオブジェクトのレポートを出力する。
// Device 解放前に呼ぶと、リークしているオブジェクトが OutputDebugString に列挙される。
void ReportLiveObjects(ID3D11Device* device);

// D3D11 オブジェクトにデバッグ名を付ける。
template <typename T>
inline void SetDebugName(T* object, const char* name) {
    if (object && name) {
        object->SetPrivateData(WKPDID_D3DDebugObjectName,
                               static_cast<UINT>(std::strlen(name)), name);
    }
}

} // namespace D3D11Debug
} // namespace D3D11CoreLib
