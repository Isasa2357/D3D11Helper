# D3D11Gpu

`D3D11Gpu` は、D3D11 の resource / view / state / sampler / shader / pipeline / transfer / copy / resolve / mipmap / binding 系 building block をまとめるモジュールです。

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
#include <D3D11Helper/D3D11Gpu/D3D11Copy.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Resolve.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Mipmap.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11View.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11State.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11BindingSet.hpp>
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
void CopyRows(void* dst, UINT dstRowPitch, const void* src, UINT srcRowPitch, UINT rowBytes, UINT height);
std::vector<uint8_t> PackRows(const void* src, UINT srcRowPitch, UINT rowBytes, UINT height);
void UnpackRows(void* dst, UINT dstRowPitch, const void* packed, UINT rowBytes, UINT height);
```

---

## D3D11TextureTransfer

`D3D11TextureTransfer` は、`D3D11Resource` / `Texture2D` と `D3D11CpuImage` の同期転送 API です。

### Readback

```cpp
D3D11CpuImage ReadbackTexture2DToCpuImage(D3D11Core& core, const D3D11Resource& srcTexture);
D3D11CpuImage ReadbackTexture2DRegionToCpuImage(D3D11Core& core, const D3D11Resource& srcTexture, const D3D11_BOX& srcBox);
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

void UpdateTexture2DFromCpuImage(D3D11Core& core, D3D11Resource& dstTexture, const D3D11CpuImage& image);
```

upload/update は、packed row pitch と padded row pitch の両方を扱えます。

---

## D3D11Copy

`D3D11Copy` は、D3D11 resource / Texture2D / Buffer の copy API を薄く包む helper です。

主な型:

```cpp
struct D3D11Box3D;
struct D3D11Texture2DRegion;
struct D3D11BufferCopyRegion;
```

主な関数:

```cpp
D3D11_BOX MakeD3D11Box(const D3D11Box3D& box);
UINT CalcD3D11Subresource(UINT mipSlice, UINT arraySlice, UINT mipLevels);

void CopyResource(ID3D11DeviceContext* context, ID3D11Resource* dst, ID3D11Resource* src);
void CopySubresource(...);

void CopyTexture2D(ID3D11DeviceContext* context, ID3D11Texture2D* dst, ID3D11Texture2D* src);
void CopyTexture2DSubresource(...);
void CopyTexture2DRegion(ID3D11DeviceContext* context, ID3D11Texture2D* dst, ID3D11Texture2D* src, const D3D11Texture2DRegion& region);

void CopyBuffer(ID3D11DeviceContext* context, ID3D11Buffer* dst, ID3D11Buffer* src);
void CopyBufferRegion(ID3D11DeviceContext* context, ID3D11Buffer* dst, ID3D11Buffer* src, const D3D11BufferCopyRegion& region);
```

`CopyTexture2DRegion()` は v1.6.0 では single-sample `Texture2D` の rectangular region copy に絞っています。MSAA texture の全体 copy は `CopyTexture2D()` / `CopyResource()` を使い、MSAA resolve は `D3D11Resolve` を使います。

詳しくは [`D3D11Copy.md`](D3D11Copy.md) を参照してください。

---

## D3D11Resolve

`D3D11Resolve` は、MSAA resource を single-sample resource に resolve する helper です。

```cpp
struct D3D11ResolveTexture2DDesc;

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
```

`ResolveTexture2D()` は、source が MSAA、destination が single-sample であること、選択された subresource のサイズが一致することを検証します。`desc.format == DXGI_FORMAT_UNKNOWN` の場合は destination texture の format を使います。

詳しくは [`D3D11Resolve.md`](D3D11Resolve.md) を参照してください。

---

## D3D11Mipmap

`D3D11Mipmap` は、D3D11 の automatic mip generation を扱う helper です。

```cpp
struct D3D11MipGenerationSrvDesc;

bool IsAutoGenerateMipsSupported(ID3D11Device* device, DXGI_FORMAT format);
bool CanGenerateMipsForTexture2D(ID3D11Device* device, ID3D11Texture2D* texture);
ComPtr<ID3D11ShaderResourceView> CreateMipGenerationTexture2DSRV(
    ID3D11Device* device,
    ID3D11Texture2D* texture,
    const D3D11MipGenerationSrvDesc& desc = {});
void GenerateMips(ID3D11DeviceContext* context, ID3D11ShaderResourceView* srv);
```

`CanGenerateMipsForTexture2D()` は、`D3D11_BIND_SHADER_RESOURCE`、`D3D11_BIND_RENDER_TARGET`、`D3D11_RESOURCE_MISC_GENERATE_MIPS`、single-sample、mip count、format support を確認します。

詳しくは [`D3D11Mipmap.md`](D3D11Mipmap.md) を参照してください。

---

## D3D11View

`D3D11View` は、既存の basic view helper を補完する advanced view helper です。

主な型:

```cpp
struct D3D11Texture2DArrayViewDesc;
struct D3D11TextureCubeViewDesc;
struct D3D11TextureCubeArrayViewDesc;
struct D3D11BufferViewDesc;
```

主な機能:

- Texture2D array SRV / UAV / RTV / DSV
- TextureCube SRV
- TextureCubeArray SRV
- typeless depth texture の DSV / SRV format mapping
- depth Texture2D / Texture2DArray DSV / SRV
- typed buffer SRV / UAV
- structured buffer SRV / UAV
- raw buffer SRV / UAV

詳しくは [`D3D11View.md`](D3D11View.md) を参照してください。

---

## D3D11State

`D3D11State` は、state object 作成 helper と common descriptor presets を提供します。

主な機能:

- `CreateSamplerState`
- `CreateRasterizerState`
- `CreateBlendState`
- `CreateDepthStencilState`
- `StatePresets::Sampler*`
- `StatePresets::Rasterizer*`
- `StatePresets::Blend*`
- `StatePresets::Depth*`

詳しくは [`D3D11State.md`](D3D11State.md) を参照してください。

---
## D3D11BindingSet

`D3D11BindingSet` は、v1.4.0 で追加された compute-stage binding helper です。

主な型:

```cpp
class D3D11ComputeBindingSet;
class D3D11ScopedComputeBindings;
```

主な自由関数:

```cpp
void BindComputeShaderResources(ID3D11DeviceContext* context, UINT startSlot, UINT count, ID3D11ShaderResourceView* const* srvs);
void BindComputeUnorderedAccessViews(ID3D11DeviceContext* context, UINT startSlot, UINT count, ID3D11UnorderedAccessView* const* uavs, const UINT* initialCounts = nullptr);
void BindComputeConstantBuffers(ID3D11DeviceContext* context, UINT startSlot, UINT count, ID3D11Buffer* const* buffers);
void BindComputeSamplers(ID3D11DeviceContext* context, UINT startSlot, UINT count, ID3D11SamplerState* const* samplers);

void UnbindComputeShaderResources(ID3D11DeviceContext* context, UINT startSlot, UINT count);
void UnbindComputeUnorderedAccessViews(ID3D11DeviceContext* context, UINT startSlot, UINT count);
void UnbindComputeConstantBuffers(ID3D11DeviceContext* context, UINT startSlot, UINT count);
void UnbindComputeSamplers(ID3D11DeviceContext* context, UINT startSlot, UINT count);
void D3D11UnbindComputeResources(ID3D11DeviceContext* context);
```

`D3D11ComputeBindingSet` は非所有の raw pointer を保持します。`D3D11ScopedComputeBindings` は previous state だけを `ComPtr` で保持して、スコープ終了時または `Restore()` で復元します。

詳しくは [`D3D11BindingSet.md`](D3D11BindingSet.md) を参照してください。

---

## v1.7.0 View / State scope

`v1.7.0` の view / state API は、D3D11 の view object と state object を薄く補助する baseline に絞っています。

対応:

- Texture2D array views
- cube / cube-array SRVs
- depth DSV/SRV format mapping
- depth Texture2D and Texture2DArray DSV/SRV helpers
- typed / structured / raw buffer views
- common state descriptor presets

非対応:

- graphics-stage binding set
- material system
- render graph integration
- automatic view cache
- descriptor heap abstraction
- shader reflection based binding

---
## v1.6.0 Copy / Resolve / Mipmap scope

`v1.6.0` の copy / resolve / mipmap API は、D3D11 の resource 操作を薄く包む baseline に絞っています。

対応:

- full resource copy
- explicit subresource copy
- Texture2D full copy
- Texture2D single-sample rectangular region copy
- buffer full copy
- buffer byte-range copy
- MSAA Texture2D resolve
- automatic mip generation through GenerateMips

非対応:

- CPU readback / upload beyond D3D11TextureTransfer
- image / video file I/O
- format conversion / colorspace conversion
- async copy scheduling
- render graph / automatic hazard tracking
- depth/stencil resolve

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

## v1.4.0 Binding scope

`v1.4.0` の binding API は compute stage の baseline に絞っています。

対応:

- CS SRV
- CS UAV
- CS constant buffer
- CS sampler
- scoped previous-state restore
- full compute-stage unbind
- slot/range validation

非対応:

- graphics-stage binding set
- descriptor heap abstraction
- shader reflection による auto binding
- resource lifetime tracking
- material system / renderer framework

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
