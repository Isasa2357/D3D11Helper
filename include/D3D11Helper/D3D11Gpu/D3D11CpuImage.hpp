#pragma once
//
// D3D11CpuImage.hpp - CPU-side image buffer for D3D11 Texture2D transfer
//
// This is a raw CPU memory container, not an image-file abstraction. It is used
// by D3D11TextureTransfer as the DirectX/DXGI-only boundary for higher-level
// file/media I/O libraries.
//
#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>

#include <cstdint>
#include <vector>

namespace D3D11CoreLib {

struct D3D11CpuImagePlane {
    UINT width = 0;
    UINT height = 0;
    UINT rowPitch = 0;
    UINT64 offsetBytes = 0;
};

struct D3D11CpuImage {
    UINT width = 0;
    UINT height = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

    std::vector<D3D11CpuImagePlane> planes;
    std::vector<uint8_t> pixels;

    bool Empty() const noexcept;
    UINT PlaneCount() const noexcept;
    UINT64 SizeBytes() const noexcept;
};

bool IsSinglePlaneCpuImageFormat(DXGI_FORMAT format) noexcept;

UINT GetPackedRowPitch(UINT width, DXGI_FORMAT format);
UINT64 GetRequiredCpuImageSize(
    UINT width,
    UINT height,
    DXGI_FORMAT format,
    UINT rowPitch = 0);

D3D11CpuImage CreateCpuImage(
    UINT width,
    UINT height,
    DXGI_FORMAT format,
    UINT rowPitch = 0);

void ValidateCpuImage(
    const D3D11CpuImage& image,
    const char* functionName);

void CopyRows(
    void* dst,
    UINT dstRowPitch,
    const void* src,
    UINT srcRowPitch,
    UINT rowBytes,
    UINT height);

std::vector<uint8_t> PackRows(
    const void* src,
    UINT srcRowPitch,
    UINT rowBytes,
    UINT height);

void UnpackRows(
    void* dst,
    UINT dstRowPitch,
    const void* packed,
    UINT rowBytes,
    UINT height);

} // namespace D3D11CoreLib
