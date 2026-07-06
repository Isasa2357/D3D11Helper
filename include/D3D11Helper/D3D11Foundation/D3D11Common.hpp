#pragma once
//
// D3D11Common.hpp - common Direct3D / DXGI includes
//
// This header belongs to D3D11Foundation in the v1.1.0 architecture.
// The legacy path D3D11Core/D3D11Common.hpp remains as a compatibility wrapper.
//
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <wrl/client.h>
#include <d3d11_4.h>
#include <dxgi1_6.h>

#include <cstdint>
#include <string>
#include <utility>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxcompiler.lib")

namespace D3D11CoreLib {
template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;
} // namespace D3D11CoreLib
