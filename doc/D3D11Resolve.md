# D3D11Resolve

`D3D11Resolve` は、D3D11 の MSAA resolve 操作を薄く包む helper です。

これは `D3D11Gpu` モジュールの一部です。`ID3D11DeviceContext::ResolveSubresource()` を扱いやすくするための小さな wrapper であり、post-process や filtering pipeline ではありません。

## Include

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11Resolve.hpp>
```

`D3D11Gpu.hpp` からも include されます。

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
```

---

## Types

```cpp
struct D3D11ResolveTexture2DDesc {
    UINT srcMip = 0;
    UINT srcArraySlice = 0;
    UINT dstMip = 0;
    UINT dstArraySlice = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
};
```

`format == DXGI_FORMAT_UNKNOWN` の場合、`ResolveTexture2D()` は destination texture の format を使用します。

---

## Raw resolve

```cpp
void ResolveSubresource(
    ID3D11DeviceContext* context,
    ID3D11Resource* dst,
    UINT dstSubresource,
    ID3D11Resource* src,
    UINT srcSubresource,
    DXGI_FORMAT format);
```

`ResolveSubresource()` は D3D11 の raw API に近い wrapper です。

主な validation:

- context / dst / src が null ではない
- format が `DXGI_FORMAT_UNKNOWN` ではない

resource compatibility、typeless format の扱い、depth/stencil resolve の可否などは D3D11 runtime の validation に従います。

---

## Texture2D resolve

```cpp
void ResolveTexture2D(
    ID3D11DeviceContext* context,
    ID3D11Texture2D* dst,
    ID3D11Texture2D* src,
    const D3D11ResolveTexture2DDesc& desc = {});
```

`ResolveTexture2D()` は `Texture2D` に特化した convenience helper です。

主な validation:

- source texture は MSAA
- destination texture は single-sample
- source / destination mip が範囲内
- source / destination array slice が範囲内
- 選択された source / destination subresource の width / height が一致
- resolve format が `DXGI_FORMAT_UNKNOWN` ではない

---

## Example

```cpp
D3D11ResolveTexture2DDesc desc = {};
desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;

ResolveTexture2D(
    core->GetImmediateContext(),
    singleSampleTexture.Get(),
    msaaTexture.Get(),
    desc);
```

format を省略する場合:

```cpp
ResolveTexture2D(ctx, singleSampleTexture.Get(), msaaTexture.Get());
```

この場合は destination texture の format が使われます。

---

## Scope

対応:

- MSAA `Texture2D` から single-sample `Texture2D` への resolve
- mip / array slice 指定
- raw `ResolveSubresource()` wrapper

非対応:

- depth/stencil resolve convenience helper
- shader-based custom resolve
- format conversion
- downsample / filtering
- automatic render-target state management
