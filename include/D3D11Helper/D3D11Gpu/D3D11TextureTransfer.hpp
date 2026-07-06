#pragma once
//
// D3D11TextureTransfer.hpp - Texture2D <-> D3D11CpuImage transfer helpers
//
// This module deliberately does not perform file I/O or media encoding.
// It only bridges D3D11 Texture2D resources and CPU memory so higher-level
// libraries can implement PNG/JPEG/DDS/video backends on top of D3D11Helper.
//
#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11CpuImage.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Resource.hpp>

namespace D3D11CoreLib {

// Synchronously copies mip 0 / array slice 0 of a Texture2D to CPU memory.
// Supported in v1.2.0:
//   - Texture2D
//   - single-plane, non-compressed formats supported by D3D11CpuImage
//   - non-MSAA textures
//   - mip 0 / array slice 0
D3D11CpuImage ReadbackTexture2DToCpuImage(
    D3D11Core& core,
    const D3D11Resource& srcTexture);

// Synchronously copies a region of mip 0 / array slice 0 of a Texture2D to CPU memory.
// For 2D textures, srcBox.front must be 0 and srcBox.back must be 1.
D3D11CpuImage ReadbackTexture2DRegionToCpuImage(
    D3D11Core& core,
    const D3D11Resource& srcTexture,
    const D3D11_BOX& srcBox);

// Creates a Texture2D from a CPU image.
// No format conversion is performed; the D3D11 texture format equals image.format.
D3D11Resource CreateTexture2DFromCpuImage(
    D3D11Core& core,
    const D3D11CpuImage& image,
    UINT bindFlags = D3D11_BIND_SHADER_RESOURCE,
    D3D11_USAGE usage = D3D11_USAGE_DEFAULT,
    UINT miscFlags = 0);

// Updates an existing Texture2D from a CPU image.
// DEFAULT textures use UpdateSubresource; DYNAMIC textures use Map/Unmap.
void UpdateTexture2DFromCpuImage(
    D3D11Core& core,
    D3D11Resource& dstTexture,
    const D3D11CpuImage& image);

} // namespace D3D11CoreLib
