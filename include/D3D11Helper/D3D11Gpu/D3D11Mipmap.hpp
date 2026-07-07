#pragma once
//
// D3D11Mipmap.hpp - Thin D3D11 automatic mip generation helpers
//
#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>

namespace D3D11CoreLib {

struct D3D11MipGenerationSrvDesc {
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT mostDetailedMip = 0;
    UINT mipLevels = UINT(-1);
};

bool IsAutoGenerateMipsSupported(ID3D11Device* device, DXGI_FORMAT format);
bool CanGenerateMipsForTexture2D(ID3D11Device* device, ID3D11Texture2D* texture);

ComPtr<ID3D11ShaderResourceView> CreateMipGenerationTexture2DSRV(
    ID3D11Device* device,
    ID3D11Texture2D* texture,
    const D3D11MipGenerationSrvDesc& desc = {});

void GenerateMips(ID3D11DeviceContext* context, ID3D11ShaderResourceView* srv);

} // namespace D3D11CoreLib
