#pragma once
//
// DxgiUtil.hpp - DXGI / LUID 小ユーティリティ
//

#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>

namespace D3D11CoreLib {

// 2つの LUID が同一かを比較する
inline bool LuidEquals(const LUID& a, const LUID& b) {
    return a.HighPart == b.HighPart && a.LowPart == b.LowPart;
}

// "High,Low" 形式の文字列にする
std::wstring LuidToWString(const LUID& luid);

} // namespace D3D11CoreLib
