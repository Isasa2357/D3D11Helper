#pragma once
//
// D3D11Helpers.hpp
// すべて第一引数に D3D11Core& を取る自由関数群。
// 上位サブシステムは「core を1つ渡して1行呼ぶ」だけで D3D11 リソースを得られる。
//
// D3D12Helpers との対応:
//   D3D12 では Upload Buffer + Copy Queue + Barrier + Fence 待ちが必要だったが、
//   D3D11 では UpdateSubresource / 初期データ付き生成で済むため大幅に簡潔。
//   UploadBuffer / UploadRing / RecordUpload 系は不要。
//
#include "../D3D11Core/D3D11Core.hpp"
#include "../D3D11Core/D3D11FormatUtil.hpp"
#include "D3D11Resource.hpp"

#include <cstdint>
#include <vector>

namespace D3D11CoreLib {

// --------------------------------------------------------------------------
// バッファ生成
// --------------------------------------------------------------------------
D3D11Resource CreateBuffer(
    D3D11Core& core,
    UINT sizeBytes,
    D3D11_USAGE usage           = D3D11_USAGE_DEFAULT,
    UINT bindFlags              = 0,
    UINT cpuAccessFlags         = 0,
    UINT miscFlags              = 0,
    const void* initialData     = nullptr,
    UINT structureByteStride    = 0);

// Structured Buffer 生成（SRV/UAV 向け）。
D3D11Resource CreateStructuredBuffer(
    D3D11Core& core,
    UINT elementCount,
    UINT elementStride,
    D3D11_USAGE usage     = D3D11_USAGE_DEFAULT,
    UINT bindFlags        = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
    const void* initialData = nullptr);

// Constant Buffer 生成。
D3D11Resource CreateConstantBuffer(
    D3D11Core& core,
    UINT sizeBytes,
    const void* initialData = nullptr);

// --------------------------------------------------------------------------
// テクスチャ生成（空）
// --------------------------------------------------------------------------
D3D11Resource CreateTexture2D(
    D3D11Core& core,
    UINT width, UINT height, DXGI_FORMAT format,
    UINT bindFlags     = D3D11_BIND_SHADER_RESOURCE,
    D3D11_USAGE usage  = D3D11_USAGE_DEFAULT,
    UINT miscFlags     = 0,
    UINT16 arraySize   = 1,
    UINT16 mipLevels   = 1);

// --------------------------------------------------------------------------
// CPU 配列 → Texture（同期）
//   単一 subresource の non-planar 2D Texture 用。
//   mipmapped / array texture など複数 subresource を持つ texture には
//   CreateTexture2DFromSubresources / UpdateTextureSubresources を使う。
// --------------------------------------------------------------------------
D3D11Resource CreateTexture2DFromMemory(
    D3D11Core& core,
    const void* data,
    UINT width, UINT height, DXGI_FORMAT format,
    UINT srcRowPitch = 0, // 0 なら width * BytesPerPixel(format)
    UINT bindFlags   = D3D11_BIND_SHADER_RESOURCE);

// RGBA8（4byte/px）配列から作る。
D3D11Resource CreateTexture2DFromRGBA(
    D3D11Core& core,
    const uint8_t* rgba, UINT width, UINT height,
    UINT bindFlags = D3D11_BIND_SHADER_RESOURCE);

// RGB（3byte/px）配列を内部で RGBA8 へ展開して作る。
D3D11Resource CreateTexture2DFromRGB(
    D3D11Core& core,
    const uint8_t* rgb, UINT width, UINT height,
    uint8_t alpha = 255,
    UINT bindFlags = D3D11_BIND_SHADER_RESOURCE);

// RGB → RGBA8 展開ユーティリティ。
std::vector<uint8_t> ExpandRGBtoRGBA(
    const uint8_t* rgb, UINT width, UINT height, uint8_t alpha = 255);

struct D3D11TextureSubresourceData {
    const void* data = nullptr;
    UINT rowPitch = 0;   // 0 なら mip width * BytesPerPixel(format)
    UINT slicePitch = 0; // 0 なら rowPitch * mip height
};

// 複数 subresource を初期データ付きで生成する。
// subresourceCount は mipLevels * arraySize と一致する必要がある。
// D3D11 の汎用 helper では planar format を扱わない。
D3D11Resource CreateTexture2DFromSubresources(
    D3D11Core& core,
    UINT width,
    UINT height,
    DXGI_FORMAT format,
    const D3D11TextureSubresourceData* subresources,
    UINT subresourceCount,
    UINT bindFlags = D3D11_BIND_SHADER_RESOURCE,
    D3D11_USAGE usage = D3D11_USAGE_DEFAULT,
    UINT miscFlags = 0,
    UINT16 arraySize = 1,
    UINT16 mipLevels = 1);

// --------------------------------------------------------------------------
// テクスチャ更新（既存テクスチャへの書き込み）
//   D3D11_USAGE_DEFAULT のテクスチャには UpdateSubresource を使う。
//   D3D11_USAGE_DYNAMIC のテクスチャには Map/Unmap を使う。
// --------------------------------------------------------------------------
void UpdateTexture2D(
    D3D11Core& core,
    D3D11Resource& dstTexture,
    const void* data,
    UINT width, UINT height, DXGI_FORMAT format,
    UINT srcRowPitch = 0);

// 複数 subresource を更新する。
// D3D11 の汎用 helper では planar format を扱わない。
void UpdateTextureSubresources(
    D3D11Core& core,
    D3D11Resource& dstTexture,
    const D3D11TextureSubresourceData* subresources,
    UINT firstSubresource,
    UINT subresourceCount);

// --------------------------------------------------------------------------
// View 作成ヘルパ
// --------------------------------------------------------------------------

// Full-desc 版。特殊 view / array view / typed view などを呼び出し側が明示的に指定する。
ComPtr<ID3D11ShaderResourceView> CreateSrv(
    D3D11Core& core,
    ID3D11Resource* resource,
    const D3D11_SHADER_RESOURCE_VIEW_DESC& desc);

ComPtr<ID3D11UnorderedAccessView> CreateUav(
    D3D11Core& core,
    ID3D11Resource* resource,
    const D3D11_UNORDERED_ACCESS_VIEW_DESC& desc);

ComPtr<ID3D11RenderTargetView> CreateRtv(
    D3D11Core& core,
    ID3D11Resource* resource,
    const D3D11_RENDER_TARGET_VIEW_DESC& desc);

ComPtr<ID3D11DepthStencilView> CreateDsv(
    D3D11Core& core,
    ID3D11Resource* resource,
    const D3D11_DEPTH_STENCIL_VIEW_DESC& desc);

// Texture2D 用 SRV を作る。format に DXGI_FORMAT_UNKNOWN を渡すと texture の format を使う。
ComPtr<ID3D11ShaderResourceView> CreateTexture2DSrv(
    D3D11Core& core,
    const D3D11Resource& texture,
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);

// Texture2D 用 UAV を作る。
ComPtr<ID3D11UnorderedAccessView> CreateTexture2DUav(
    D3D11Core& core,
    const D3D11Resource& texture,
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN,
    UINT mipSlice = 0);

// Buffer 用 SRV を作る。
// format == DXGI_FORMAT_UNKNOWN のときは Structured Buffer view。
// format != DXGI_FORMAT_UNKNOWN のときは Typed Buffer view。
ComPtr<ID3D11ShaderResourceView> CreateBufferSrv(
    D3D11Core& core,
    const D3D11Resource& buffer,
    UINT firstElement,
    UINT numElements,
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);

// Buffer 用 UAV を作る。
// format == DXGI_FORMAT_UNKNOWN のときは Structured Buffer view。
// format != DXGI_FORMAT_UNKNOWN のときは Typed Buffer view。
ComPtr<ID3D11UnorderedAccessView> CreateBufferUav(
    D3D11Core& core,
    const D3D11Resource& buffer,
    UINT firstElement,
    UINT numElements,
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN,
    UINT flags = 0);

// Texture2D 用 RTV を作る。
ComPtr<ID3D11RenderTargetView> CreateTexture2DRtv(
    D3D11Core& core,
    const D3D11Resource& texture,
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN,
    UINT mipSlice = 0);

// Texture2D 用 DSV を作る。
ComPtr<ID3D11DepthStencilView> CreateTexture2DDsv(
    D3D11Core& core,
    const D3D11Resource& texture,
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN,
    UINT mipSlice = 0);

// --------------------------------------------------------------------------
// Sampler 作成ヘルパ
// --------------------------------------------------------------------------
D3D11_SAMPLER_DESC MakeLinearClampSamplerDesc();
D3D11_SAMPLER_DESC MakePointClampSamplerDesc();

ComPtr<ID3D11SamplerState> CreateSampler(
    D3D11Core& core,
    const D3D11_SAMPLER_DESC& desc);

// --------------------------------------------------------------------------
// 共有リソース生成
//   D3D11/D3D12 間で共有するテクスチャを D3D11 側で作成する。
//   SharedFence 用は SHARED_NTHANDLE | SHARED。
//   KeyedMutex 用は SHARED_NTHANDLE | SHARED_KEYEDMUTEX。
// --------------------------------------------------------------------------
enum class D3D11SharedTextureSyncMode {
    SharedFence,
    KeyedMutex
};

D3D11Resource CreateSharedTexture2D(
    D3D11Core& core,
    UINT width, UINT height, DXGI_FORMAT format,
    UINT bindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
    D3D11SharedTextureSyncMode syncMode = D3D11SharedTextureSyncMode::SharedFence);

} // namespace D3D11CoreLib
