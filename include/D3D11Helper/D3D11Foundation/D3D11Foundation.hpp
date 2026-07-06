#pragma once
//
// D3D11Foundation.hpp - v1.1.0 module umbrella
//
// Foundation groups DirectX/DXGI-only utilities that do not own a D3D11
// device/context.  This is an additive umbrella for the architecture migration;
// existing headers remain in their current locations until the following
// migration steps move or wrap them.
//

#include <D3D11Helper/D3D11Core/D3D11Common.hpp>
#include <D3D11Helper/D3D11Core/ThrowIfFailed.hpp>
#include <D3D11Helper/D3D11Core/DxgiUtil.hpp>
#include <D3D11Helper/D3D11Core/DxgiAdapterSelector.hpp>
#include <D3D11Helper/D3D11Core/D3D11FormatUtil.hpp>

namespace D3D11CoreLib {
namespace Foundation {

} // namespace Foundation
} // namespace D3D11CoreLib
