#pragma once
//
// D3D11View.hpp - advanced D3D11 view helper functions
//
// v1.7.0 adds focused helpers for array, cube, depth, typed buffer,
// structured buffer, and raw buffer views. These helpers remain thin wrappers
// over D3D11 descriptor creation and do not own resource lifetime.
//
#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>

namespace D3D11CoreLib {

struct D3D11Texture2DArrayViewDesc {
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT mipSlice = 0;          // RTV / UAV / DSV
    UINT mostDetailedMip = 0;   // SRV
    UINT mipLevels = UINT(-1);  // SRV, UINT(-1) = all remaining mips
    UINT firstArraySlice = 0;
    UINT arraySize = UINT(-1);  // UINT(-1) = all remaining array slices
};

struct D3D11TextureCubeViewDesc {
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT mostDetailedMip = 0;
    UINT mipLevels = UINT(-1);
};

struct D3D11TextureCubeArrayViewDesc {
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT mostDetailedMip = 0;
    UINT mipLevels = UINT(-1);
    UINT firstCube = 0;
    UINT cubeCount = UINT(-1);
};

struct D3D11BufferViewDesc {
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT firstElement = 0;
    UINT numElements = 0;
    UINT flags = 0;
};

ComPtr<ID3D11ShaderResourceView> CreateTexture2DArraySrv(
    ID3D11Device* device,
    ID3D11Texture2D* texture,
    const D3D11Texture2DArrayViewDesc& desc = {});

ComPtr<ID3D11UnorderedAccessView> CreateTexture2DArrayUav(
    ID3D11Device* device,
    ID3D11Texture2D* texture,
    const D3D11Texture2DArrayViewDesc& desc = {});

ComPtr<ID3D11RenderTargetView> CreateTexture2DArrayRtv(
    ID3D11Device* device,
    ID3D11Texture2D* texture,
    const D3D11Texture2DArrayViewDesc& desc = {});

ComPtr<ID3D11DepthStencilView> CreateTexture2DArrayDsv(
    ID3D11Device* device,
    ID3D11Texture2D* texture,
    const D3D11Texture2DArrayViewDesc& desc = {});

ComPtr<ID3D11ShaderResourceView> CreateTextureCubeSrv(
    ID3D11Device* device,
    ID3D11Texture2D* texture,
    const D3D11TextureCubeViewDesc& desc = {});

ComPtr<ID3D11ShaderResourceView> CreateTextureCubeArraySrv(
    ID3D11Device* device,
    ID3D11Texture2D* texture,
    const D3D11TextureCubeArrayViewDesc& desc = {});

bool IsDepthStencilViewFormat(DXGI_FORMAT format) noexcept;
bool IsTypelessDepthTextureFormat(DXGI_FORMAT format) noexcept;

DXGI_FORMAT GetTypelessDepthTextureFormat(DXGI_FORMAT format) noexcept;
DXGI_FORMAT GetDepthStencilViewFormat(DXGI_FORMAT format) noexcept;
DXGI_FORMAT GetDepthShaderResourceViewFormat(DXGI_FORMAT format) noexcept;
DXGI_FORMAT GetStencilShaderResourceViewFormat(DXGI_FORMAT format) noexcept;

ComPtr<ID3D11DepthStencilView> CreateDepthTexture2DDsv(
    ID3D11Device* device,
    ID3D11Texture2D* texture,
    UINT mipSlice = 0,
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN,
    UINT flags = 0);

ComPtr<ID3D11ShaderResourceView> CreateDepthTexture2DSrv(
    ID3D11Device* device,
    ID3D11Texture2D* texture,
    UINT mostDetailedMip = 0,
    UINT mipLevels = UINT(-1),
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);

ComPtr<ID3D11ShaderResourceView> CreateTypedBufferSrv(
    ID3D11Device* device,
    ID3D11Buffer* buffer,
    DXGI_FORMAT format,
    UINT firstElement,
    UINT numElements);

ComPtr<ID3D11UnorderedAccessView> CreateTypedBufferUav(
    ID3D11Device* device,
    ID3D11Buffer* buffer,
    DXGI_FORMAT format,
    UINT firstElement,
    UINT numElements,
    UINT flags = 0);

ComPtr<ID3D11ShaderResourceView> CreateStructuredBufferSrv(
    ID3D11Device* device,
    ID3D11Buffer* buffer,
    UINT firstElement,
    UINT numElements);

ComPtr<ID3D11UnorderedAccessView> CreateStructuredBufferUav(
    ID3D11Device* device,
    ID3D11Buffer* buffer,
    UINT firstElement,
    UINT numElements,
    UINT flags = 0);

ComPtr<ID3D11ShaderResourceView> CreateRawBufferSrv(
    ID3D11Device* device,
    ID3D11Buffer* buffer,
    UINT firstElement,
    UINT numElements);

ComPtr<ID3D11UnorderedAccessView> CreateRawBufferUav(
    ID3D11Device* device,
    ID3D11Buffer* buffer,
    UINT firstElement,
    UINT numElements,
    UINT flags = D3D11_BUFFER_UAV_FLAG_RAW);

} // namespace D3D11CoreLib
