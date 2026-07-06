# D3D11Gpu

`D3D11Gpu` は、D3D11 の resource / view / sampler / shader / pipeline / transfer 系 building block をまとめるモジュールです。

旧 `D3D11Framework` の主な移行先です。

## Include

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
```

個別 include:

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11Resource.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Helpers.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11StagingBuffer.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11ComputePipeline.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11GraphicsPipeline.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11ShaderCompiler.hpp>
```

## D3D11Resource

`ID3D11Resource` の薄い wrapper です。Texture2D / Buffer として取得する helper を持ちます。

```cpp
ID3D11Resource*  Get() const;
ID3D11Texture2D* AsTexture2D();
ID3D11Buffer*    AsBuffer();

D3D11_TEXTURE2D_DESC GetTexture2DDesc() const;
D3D11_BUFFER_DESC    GetBufferDesc() const;

UINT GetWidth() const;
UINT GetHeight() const;
DXGI_FORMAT GetFormat() const;
```

## D3D11Helpers

`D3D11Core&` を第一引数に取る自由関数群です。

主な機能:

- `CreateBuffer`
- `CreateStructuredBuffer`
- `CreateConstantBuffer`
- `CreateTexture2D`
- `CreateTexture2DFromMemory`
- `CreateTexture2DFromRGBA`
- `CreateTexture2DFromRGB`
- `CreateTexture2DFromSubresources`
- `UpdateTexture2D`
- `UpdateTextureSubresources`
- `CreateSrv`
- `CreateUav`
- `CreateRtv`
- `CreateDsv`
- `CreateTexture2DSrv`
- `CreateTexture2DUav`
- `CreateTexture2DRtv`
- `CreateTexture2DDsv`
- `CreateBufferSrv`
- `CreateBufferUav`
- `CreateSampler`

`CreateSharedTexture2D` は現在は互換上 `D3D11Helpers` に残っていますが、意味上は Interop 寄りの helper です。

## D3D11StagingBuffer

GPU→CPU readback 用の staging buffer / staging texture です。

```cpp
void InitializeAsBuffer(ID3D11Device* device, UINT64 sizeBytes);
void InitializeAsTexture2D(ID3D11Device* device, UINT width, UINT height, DXGI_FORMAT format);
const void* Map(ID3D11DeviceContext* ctx);
void Unmap(ID3D11DeviceContext* ctx);
UINT GetMappedRowPitch() const;
```

## Pipeline

### D3D11ComputePipeline

Compute Shader を保持し、`Bind` / `Dispatch` / `Unbind` を提供します。

### D3D11GraphicsPipeline

VS / PS / InputLayout / RasterizerState / BlendState / DepthStencilState をまとめます。

## ShaderCompiler

- `.cso` 読み込み
- D3DCompile
- DXC compile
- includeDirs / defines 対応

## 互換パス

v1.x では旧パスも使えます。

```cpp
#include <D3D11Helper/D3D11Framework/D3D11Framework.hpp>
```

新規コードでは `D3D11Gpu/` を推奨します。
