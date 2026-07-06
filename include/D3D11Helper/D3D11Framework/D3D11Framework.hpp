#pragma once
//
// D3D11Framework.hpp - compatibility umbrella
//
// v1.1.0 splits the old Framework layer into:
//   - D3D11Gpu          : resources, views, shaders, pipelines, transfer helpers
//   - D3D11Presentation : swap-chain and presentation helpers
//
// Keep this umbrella for source compatibility.  New code should include
// <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp> and/or
// <D3D11Helper/D3D11Presentation/D3D11Presentation.hpp> directly.
//
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
#include <D3D11Helper/D3D11Presentation/D3D11SwapChainHelper.hpp>
