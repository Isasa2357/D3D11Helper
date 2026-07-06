#pragma once
//
// D3D11FormatUtil.hpp - DXGI_FORMAT 判定ヘルパ
//
#include <D3D11Helper/D3D11Core/D3D11Common.hpp>

namespace D3D11CoreLib {
namespace FormatUtil {

bool IsDepthFormat(DXGI_FORMAT format);
bool IsTypelessFormat(DXGI_FORMAT format);
bool IsBlockCompressedFormat(DXGI_FORMAT format);

// YUV / video 系フォーマットかどうかを返す。
// packed / planar の両方を含む。
bool IsYuvFormat(DXGI_FORMAT format);

// 複数 plane を持つフォーマットかどうかを返す。
// packed YUV は false、NV12 / P010 などの planar format は true。
bool IsPlanarFormat(DXGI_FORMAT format);

// 2D テクスチャの width / height の両方に偶数制約を持つ代表的な
// サブサンプリングフォーマットかどうかを返す。
bool RequiresEvenSize(DXGI_FORMAT format);

// 既知 DXGI_FORMAT の plane 数を返す。
// DXGI_FORMAT_UNKNOWN は 0、それ以外の通常 format は 1、planar format は 2 以上。
// D3D11 ではこの値は format trait であり、D3D12 の PlaneSlice 付き
// subresource index と同一視しないこと。
UINT GetKnownPlaneCount(DXGI_FORMAT format);

// 1ピクセルあたりのビット数（ブロック圧縮はブロック平均ではなく代表値）。
UINT BitsPerPixel(DXGI_FORMAT format);
// バイト数（端数切り上げ）。BC 形式は 0 を返す（ブロック単位で扱うべきため）。
UINT BytesPerPixel(DXGI_FORMAT format);

} // namespace FormatUtil
} // namespace D3D11CoreLib
