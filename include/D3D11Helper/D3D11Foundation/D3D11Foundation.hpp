#pragma once
//
// D3D11Foundation.hpp - Foundation module umbrella
//
// Foundation groups DirectX/DXGI-only utilities that do not own a D3D11
// device/context.  The old D3D11Core/* utility paths remain as compatibility
// wrappers during the v1.x architecture migration.
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
