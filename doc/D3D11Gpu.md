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
#include <D3D11Helper/D3D11Gpu/D3D11CpuImage.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11TextureTransfer.hpp>
```

---

## D3D11Resource

`ID3D11Resource` の薄い wrapper です。Texture2D / Buffer として取得する helper を持ちます。

```cpp
ID3D11Resource*  Get() const;
ID3D11Texture2D* AsTexture2D();
ID3D11Buffer*    AsBuffer();

D3D11_TEXTURE2D_DESC GetTexture2DDesc() const;
D3D11_BUFFER_DESC    GetBufferDesc();

UINT GetWidth() const;
UINT GetHeight() const;
DXGI_FORMAT GetFormat() const;
```

---

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

---

## D3D11CpuImage

`D3D11CpuImage` は、D3D11 texture と CPU メモリの橋渡しに使う CPU 側画像バッファです。

これは PNG/JPEG/BMP/DDS などのファイル形式ではなく、**raw pixel memory** を表します。上位ライブラリは `D3D11CpuImage` を受け取って、必要に応じて WIC / Media Foundation / NVENC / FFmpeg などへ接続します。

```cpp
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
```

### Utility

```cpp
bool IsSinglePlaneCpuImageFormat(DXGI_FORMAT format);
UINT GetPackedRowPitch(UINT width, DXGI_FORMAT format);
UINT64 GetRequiredCpuImageSize(UINT width, UINT height, DXGI_FORMAT format, UINT rowPitch = 0);

D3D11CpuImage CreateCpuImage(UINT width, UINT height, DXGI_FORMAT format, UINT rowPitch = 0);
void ValidateCpuImage(const D3D11CpuImage& image, const char* functionName);
```

### Row pitch helper

```cpp
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
```

---

## D3D11TextureTransfer

`D3D11TextureTransfer` は、`D3D11Resource` / `Texture2D` と `D3D11CpuImage` の同期転送 API です。

### Readback

```cpp
D3D11CpuImage ReadbackTexture2DToCpuImage(
    D3D11Core& core,
    const D3D11Resource& srcTexture);

D3D11CpuImage ReadbackTexture2DRegionToCpuImage(
    D3D11Core& core,
    const D3D11Resource& srcTexture,
    const D3D11_BOX& srcBox);
```

readback の基本動作:

1. source texture の desc を検証
2. staging texture を作成
3. `CopyResource` または `CopySubresourceRegion`
4. `D3D11Core::Flush()` で GPU 完了を待つ
5. `Map`
6. mapped row pitch を吸収して `D3D11CpuImage` に pack
7. `Unmap`

### Upload / Update

```cpp
D3D11Resource CreateTexture2DFromCpuImage(
    D3D11Core& core,
    const D3D11CpuImage& image,
    UINT bindFlags = D3D11_BIND_SHADER_RESOURCE,
    D3D11_USAGE usage = D3D11_USAGE_DEFAULT,
    UINT miscFlags = 0);

void UpdateTexture2DFromCpuImage(
    D3D11Core& core,
    D3D11Resource& dstTexture,
    const D3D11CpuImage& image);
```

upload/update は、packed row pitch と padded row pitch の両方を扱えます。

---

## v1.2.0 Transfer scope

`v1.2.0` の transfer API は、まず安全な baseline に絞っています。

対応:

- `Texture2D`
- mip 0
- array slice 0
- single-plane format
- non-block-compressed format
- non-MSAA texture readback
- packed/padded CPU row pitch

非対応:

- PNG/JPEG/BMP/DDS などのファイル形式
- MP4/H.264/HEVC などの動画形式
- Media Foundation / NVENC / FFmpeg / OpenCV 依存
- format conversion
- color-space conversion
- async readback ring
- full planar format transfer

---

## D3D11StagingBuffer

GPU→CPU readback 用の staging buffer / staging texture です。`D3D11TextureTransfer` の内部でも同じ D3D11 staging texture の考え方を使います。

```cpp
void InitializeAsBuffer(ID3D11Device* device, UINT64 sizeBytes);
void InitializeAsTexture2D(ID3D11Device* device, UINT width, UINT height, DXGI_FORMAT format);
const void* Map(ID3D11DeviceContext* ctx);
void Unmap(ID3D11DeviceContext* ctx);
UINT GetMappedRowPitch() const;
```

---

## Pipeline

### D3D11ComputePipeline

Compute Shader を保持し、`Bind` / `Dispatch` / `Unbind` を提供します。

### D3D11GraphicsPipeline

VS / PS / InputLayout / RasterizerState / BlendState / DepthStencilState をまとめます。

---

## ShaderCompiler

- `.cso` 読み込み
- D3DCompile
- DXC compile
- includeDirs / defines 対応

---

## 互換パス

v1.x では旧パスも使えます。

```cpp
#include <D3D11Helper/D3D11Framework/D3D11Framework.hpp>
```

新規コードでは `D3D11Gpu/` を推奨します。
