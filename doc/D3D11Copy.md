# D3D11Copy

`D3D11Copy` は、D3D11 resource / `Texture2D` / `Buffer` の copy 操作を薄く包む helper です。

これは `D3D11Gpu` モジュールの一部であり、CPU readback / upload、画像ファイル入出力、format conversion は扱いません。

## Include

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11Copy.hpp>
```

`D3D11Gpu.hpp` からも include されます。

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
```

---

## Types

```cpp
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
```

---

## Resource copy

```cpp
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
```

`CopyResource()` は `ID3D11DeviceContext::CopyResource()` の薄い wrapper です。`CopySubresource()` は `CopySubresourceRegion()` の薄い wrapper です。

どちらも null pointer を検証します。resource compatibility は基本的に D3D11 runtime の validation に従います。

---

## Texture2D copy

```cpp
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
```

`CopyTexture2DRegion()` は、v1.6.0 では single-sample texture の rectangular region copy に絞っています。

主な validation:

- context / dst / src が null ではない
- mip / array slice が範囲内
- region width / height が 0 ではない
- source / destination region が texture bounds 内
- source / destination texture が single-sample

---

## Buffer copy

```cpp
void CopyBuffer(
    ID3D11DeviceContext* context,
    ID3D11Buffer* dst,
    ID3D11Buffer* src);

void CopyBufferRegion(
    ID3D11DeviceContext* context,
    ID3D11Buffer* dst,
    ID3D11Buffer* src,
    const D3D11BufferCopyRegion& region);
```

`CopyBufferRegion()` は byte offset / byte size で範囲を指定します。

主な validation:

- context / dst / src が null ではない
- `sizeBytes > 0`
- source byte range が source buffer 内
- destination byte range が destination buffer 内

---

## Helper

```cpp
D3D11_BOX MakeD3D11Box(const D3D11Box3D& box);
UINT CalcD3D11Subresource(UINT mipSlice, UINT arraySlice, UINT mipLevels);
```

`CalcD3D11Subresource()` は D3D11 の `mipSlice + arraySlice * mipLevels` を計算します。`mipLevels == 0`、`mipSlice >= mipLevels`、`UINT` overflow は例外になります。

---

## Scope

対応:

- full resource copy
- explicit subresource copy
- Texture2D full copy
- Texture2D subresource copy
- Texture2D single-sample rectangular region copy
- buffer full copy
- buffer byte-range copy

非対応:

- CPU readback / upload
- image file I/O
- format conversion
- colorspace conversion
- automatic hazard tracking
- async copy scheduling
- block-compressed region convenience helper

CPU readback / upload は [`D3D11TextureTransfer.md`](D3D11TextureTransfer.md) を使います。
