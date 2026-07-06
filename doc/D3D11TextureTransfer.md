# D3D11TextureTransfer

`D3D11TextureTransfer` は、D3D11Helper v1.2.0 で追加された **Texture2D と CPU memory の同期転送 API** です。

この機能はファイル I/O ではありません。D3D11Helper 本体は Direct3D / DXGI に閉じ、PNG/JPEG/DDS/MP4/NVENC/Media Foundation などは上位ライブラリで扱う方針です。

## Include

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11CpuImage.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11TextureTransfer.hpp>
```

または:

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
```

## CpuImage を作る

```cpp
auto image = CreateCpuImage(width, height, DXGI_FORMAT_R8G8B8A8_UNORM);

for (UINT y = 0; y < image.height; ++y) {
    auto* row = image.pixels.data() + image.planes[0].offsetBytes + y * image.planes[0].rowPitch;
    // row[x * 4 + channel] = ...
}
```

`rowPitch` を明示すると padding 付き CPU image を作れます。

```cpp
auto image = CreateCpuImage(width, height, DXGI_FORMAT_R8G8B8A8_UNORM, paddedRowPitch);
```

## CpuImage から Texture2D を作る

```cpp
auto texture = CreateTexture2DFromCpuImage(
    *core,
    image,
    D3D11_BIND_SHADER_RESOURCE,
    D3D11_USAGE_DEFAULT);
```

## 既存 Texture2D を更新する

```cpp
UpdateTexture2DFromCpuImage(*core, texture, image);
```

`D3D11_USAGE_DEFAULT` texture では `UpdateSubresource` を使います。`D3D11_USAGE_DYNAMIC` texture では `Map(D3D11_MAP_WRITE_DISCARD)` を使います。

## Texture2D を CpuImage に読み戻す

```cpp
auto image = ReadbackTexture2DToCpuImage(*core, texture);
```

内部では staging texture を作成し、GPU コピー完了後に mapped row pitch を packed CPU image へ吸収します。

## Region readback

```cpp
D3D11_BOX box{};
box.left = 16;
box.top = 16;
box.right = 128;
box.bottom = 128;
box.front = 0;
box.back = 1;

auto image = ReadbackTexture2DRegionToCpuImage(*core, texture, box);
```

## Supported scope

v1.2.0 の対応範囲:

- Texture2D
- mip 0
- array slice 0
- single-plane format
- non-block-compressed format
- non-MSAA texture readback
- packed or padded CPU row pitch

## Unsupported scope

以下は意図的に v1.2.0 では対象外です。

- planar transfer for NV12/P010/P016
- block compressed formats
- MSAA readback
- mip/array-slice transfer
- format conversion
- color space conversion
- async readback ring
- file/media I/O

必要な場合は上位ライブラリまたは将来バージョンで扱います。

## Related sample

`sample/18_TextureTransfer` は、ファイル I/O を使わずに以下を行います。

1. CPU 側で RGBA8 gradient を作成
2. `CreateTexture2DFromCpuImage`
3. `ReadbackTexture2DToCpuImage`
4. pixel 一致検証
5. `UpdateTexture2DFromCpuImage`
6. 再度 readback して一致検証
