#pragma once
//
// D3D11Interop.hpp - v1.1.0 module umbrella
//
// Interop groups DirectX/DXGI-only interoperation primitives such as shared
// handles, shared resources, keyed-mutex/fence style synchronization, and
// D3D11-D3D12 resource sharing.  External API integrations such as CUDA, Varjo,
// Media Foundation, NVENC, or FFmpeg stay in upper-layer libraries.
//

#include <D3D11Helper/D3D11Core/D3D11SharedResource.hpp>
#include <D3D11Helper/D3D11Core/D3D11Fence.hpp>

namespace D3D11CoreLib {
namespace Interop {

} // namespace Interop
} // namespace D3D11CoreLib
