#pragma once
//
// D3D11TextureTransfer.hpp
//
// DirectX/DXGI-only Texture2D <-> CPU memory transfer helpers.
//
// This module deliberately does not perform file I/O, image codec work, video
// encoding, color conversion, or format conversion.  It provides the primitive
// transfer layer that higher-level libraries can build on.
//

#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11CpuImage.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Resource.hpp>

namespace D3D11CoreLib {

// -----------------------------------------------------------------------------
// GPU -> CPU
// -----------------------------------------------------------------------------

// Synchronously copies mip 0 / array slice 0 of a non-MSAA, single-plane Texture2D
// into a tightly packed D3D11CpuImage.
// Unsupported formats such as block-compressed, planar, depth/stencil, or
// typeless formats throw std::invalid_argument.
D3D11CpuImage ReadbackTexture2DToCpuImage(
    D3D11Core& core,
    const D3D11Resource& srcTexture);

// Synchronously copies a rectangular region from mip 0 / array slice 0 of a
// non-MSAA, single-plane Texture2D into a tightly packed D3D11CpuImage.
// The source box must be a 2D box: front == 0 and back == 1.
D3D11CpuImage ReadbackTexture2DRegionToCpuImage(
    D3D11Core& core,
    const D3D11Resource& srcTexture,
    const D3D11_BOX& srcBox);

// -----------------------------------------------------------------------------
// CPU -> GPU
// -----------------------------------------------------------------------------

// Creates a Texture2D from CPU image memory.
// v1.2.0 supports single-plane, non-compressed formats without format
// conversion. The image rowPitch may be tightly packed or padded.
//
// usage may be DEFAULT, IMMUTABLE, or DYNAMIC. DYNAMIC textures are created with
// D3D11_CPU_ACCESS_WRITE. STAGING textures are intentionally not created by this
// helper; use an explicit staging resource helper when needed.
D3D11Resource CreateTexture2DFromCpuImage(
    D3D11Core& core,
    const D3D11CpuImage& image,
    UINT bindFlags = D3D11_BIND_SHADER_RESOURCE,
    D3D11_USAGE usage = D3D11_USAGE_DEFAULT,
    UINT miscFlags = 0);

// Updates an existing Texture2D from CPU image memory.
// The destination texture must match image width / height / format, be
// single-plane and non-MSAA, and have usage DEFAULT or DYNAMIC.
void UpdateTexture2DFromCpuImage(
    D3D11Core& core,
    D3D11Resource& dstTexture,
    const D3D11CpuImage& image);

} // namespace D3D11CoreLib
