#pragma once
//
// D3D11Resolve.hpp - Thin D3D11 MSAA resolve helpers
//
#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>

namespace D3D11CoreLib {

struct D3D11ResolveTexture2DDesc {
    UINT srcMip = 0;
    UINT srcArraySlice = 0;
    UINT dstMip = 0;
    UINT dstArraySlice = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
};

void ResolveSubresource(
    ID3D11DeviceContext* context,
    ID3D11Resource* dst,
    UINT dstSubresource,
    ID3D11Resource* src,
    UINT srcSubresource,
    DXGI_FORMAT format);

void ResolveTexture2D(
    ID3D11DeviceContext* context,
    ID3D11Texture2D* dst,
    ID3D11Texture2D* src,
    const D3D11ResolveTexture2DDesc& desc = {});

} // namespace D3D11CoreLib
