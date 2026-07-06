#pragma once
//
// D3D11Foundation.hpp - DirectX/DXGI-only utility umbrella
//
// Foundation groups utilities that do not own a D3D11 device/context.
// This module is intentionally dependency-light and should remain usable from
// Core, Gpu, Presentation, Interop, Diagnostics, and Processing.
//
#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>
#include <D3D11Helper/D3D11Foundation/DxgiUtil.hpp>
#include <D3D11Helper/D3D11Foundation/DxgiAdapterSelector.hpp>
#include <D3D11Helper/D3D11Foundation/D3D11FormatUtil.hpp>

namespace D3D11CoreLib {
namespace Foundation {

} // namespace Foundation
} // namespace D3D11CoreLib
