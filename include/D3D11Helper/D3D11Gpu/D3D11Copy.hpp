#pragma once
//
// D3D11Copy.hpp - Thin D3D11 resource copy helpers
//
#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>

namespace D3D11CoreLib {

struct D3D11Box3D {
    UINT left = 0;
    UINT top = 0;
    UINT front = 0;
    UINT right = 0;
    UINT bottom = 0;
    UINT back = 1;
};

struct D3D11Texture2DRegion {
    UINT srcMip = 0;
    UINT srcArraySlice = 0;
    UINT dstMip = 0;
    UINT dstArraySlice = 0;
    UINT srcX = 0;
    UINT srcY = 0;
    UINT dstX = 0;
    UINT dstY = 0;
    UINT width = 0;
    UINT height = 0;
};

struct D3D11BufferCopyRegion {
    UINT srcOffset = 0;
    UINT dstOffset = 0;
    UINT sizeBytes = 0;
};

D3D11_BOX MakeD3D11Box(const D3D11Box3D& box);
UINT CalcD3D11Subresource(UINT mipSlice, UINT arraySlice, UINT mipLevels);

void CopyResource(
    ID3D11DeviceContext* context,
    ID3D11Resource* dst,
    ID3D11Resource* src);

void CopySubresource(
    ID3D11DeviceContext* context,
    ID3D11Resource* dst,
    UINT dstSubresource,
    UINT dstX,
    UINT dstY,
    UINT dstZ,
    ID3D11Resource* src,
    UINT srcSubresource,
    const D3D11_BOX* srcBox = nullptr);

void CopyTexture2D(
    ID3D11DeviceContext* context,
    ID3D11Texture2D* dst,
    ID3D11Texture2D* src);

void CopyTexture2DSubresource(
    ID3D11DeviceContext* context,
    ID3D11Texture2D* dst,
    UINT dstMip,
    UINT dstArraySlice,
    ID3D11Texture2D* src,
    UINT srcMip,
    UINT srcArraySlice);

void CopyTexture2DRegion(
    ID3D11DeviceContext* context,
    ID3D11Texture2D* dst,
    ID3D11Texture2D* src,
    const D3D11Texture2DRegion& region);

void CopyBuffer(
    ID3D11DeviceContext* context,
    ID3D11Buffer* dst,
    ID3D11Buffer* src);

void CopyBufferRegion(
    ID3D11DeviceContext* context,
    ID3D11Buffer* dst,
    ID3D11Buffer* src,
    const D3D11BufferCopyRegion& region);

} // namespace D3D11CoreLib
