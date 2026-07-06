#pragma once
//
// ThrowIfFailed.hpp
// HRESULT を例外化する。HRESULT値・呼び出し式・ファイル・行・任意メッセージを含める。
//
#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>

#include <stdexcept>
#include <string>

namespace D3D11CoreLib {

std::string HResultToHexString(HRESULT hr);

// 詳細版（マクロ経由で使う）
[[noreturn]] void ThrowHResult(HRESULT hr, const char* expr,
                               const char* file, int line, const char* message);

inline void ThrowIfFailed(HRESULT hr, const char* expr,
                          const char* file, int line, const char* message = nullptr) {
    if (FAILED(hr)) {
        ThrowHResult(hr, expr, file, line, message);
    }
}

// 簡易版
void ThrowIfFailed(HRESULT hr);
void ThrowIfFailed(HRESULT hr, const char* message);

} // namespace D3D11CoreLib

// 呼び出し箇所（式・ファイル・行）を自動付与するマクロ
#define D3D11CORE_THROW_IF_FAILED(hr) \
    ::D3D11CoreLib::ThrowIfFailed((hr), #hr, __FILE__, __LINE__)

#define D3D11CORE_THROW_IF_FAILED_MSG(hr, msg) \
    ::D3D11CoreLib::ThrowIfFailed((hr), #hr, __FILE__, __LINE__, (msg))
