#pragma once
//
// D3D11Interop.hpp - interop module umbrella
//
// Interop owns shared handles, shared resources, D3D11.4 fences, keyed-mutex
// style synchronization, and D3D11-D3D12 interop helpers.  It must remain
// DirectX/DXGI-only; CUDA/Varjo/NVENC/etc. integrations belong to upper layers.
//
#include <D3D11Helper/D3D11Interop/D3D11SharedHandle.hpp>
#include <D3D11Helper/D3D11Interop/D3D11SharedResource.hpp>
#include <D3D11Helper/D3D11Interop/D3D11SharedTexture.hpp>
#include <D3D11Helper/D3D11Interop/D3D11KeyedMutex.hpp>
#include <D3D11Helper/D3D11Interop/D3D11Fence.hpp>
#include <D3D11Helper/D3D11Interop/D3D11FenceSupport.hpp>

namespace D3D11CoreLib {
namespace Interop {

} // namespace Interop
} // namespace D3D11CoreLib
