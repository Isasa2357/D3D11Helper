# Migration Guide - D3D11Helper v1.6.0

`v1.6.0` is a backward-compatible feature release.

Existing `v1.5.0` code does not need to change.

---

## What changed

`D3D11Gpu` now includes helpers for:

- copy
- resolve
- mipmap generation

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
```

or individual headers:

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11Copy.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Resolve.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Mipmap.hpp>
```

---

## Replacing raw D3D11 copy calls

Before:

```cpp
context->CopyResource(dst.Get(), src.Get());
```

After:

```cpp
CopyResource(context, dst.Get(), src.Get());
```

The helper performs basic null pointer validation before calling the D3D11 API.

---

## Replacing manual Texture2D region copy

Before:

```cpp
D3D11_BOX box = {};
box.left = srcX;
box.top = srcY;
box.right = srcX + width;
box.bottom = srcY + height;
box.front = 0;
box.back = 1;

context->CopySubresourceRegion(dst.Get(), 0, dstX, dstY, 0, src.Get(), 0, &box);
```

After:

```cpp
D3D11Texture2DRegion region = {};
region.srcX = srcX;
region.srcY = srcY;
region.dstX = dstX;
region.dstY = dstY;
region.width = width;
region.height = height;

CopyTexture2DRegion(context, dst.Get(), src.Get(), region);
```

`CopyTexture2DRegion()` validates mip / array slice / bounds and is limited to single-sample textures in `v1.6.0`.

---

## Replacing manual buffer region copy

Before:

```cpp
D3D11_BOX box = {};
box.left = srcOffset;
box.right = srcOffset + sizeBytes;
box.top = 0;
box.bottom = 1;
box.front = 0;
box.back = 1;

context->CopySubresourceRegion(dst.Get(), 0, dstOffset, 0, 0, src.Get(), 0, &box);
```

After:

```cpp
D3D11BufferCopyRegion region = {};
region.srcOffset = srcOffset;
region.dstOffset = dstOffset;
region.sizeBytes = sizeBytes;

CopyBufferRegion(context, dst.Get(), src.Get(), region);
```

---

## Replacing raw MSAA resolve

Before:

```cpp
context->ResolveSubresource(dst.Get(), 0, src.Get(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);
```

After:

```cpp
ResolveTexture2D(context, dst.Get(), src.Get());
```

or explicitly:

```cpp
D3D11ResolveTexture2DDesc desc = {};
desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
ResolveTexture2D(context, dst.Get(), src.Get(), desc);
```

If `desc.format == DXGI_FORMAT_UNKNOWN`, the destination texture format is used.

---

## Using automatic mip generation

Texture creation must satisfy D3D11 automatic mip generation requirements:

```cpp
BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
MipLevels > 1;
SampleDesc.Count = 1;
```

Then:

```cpp
if (CanGenerateMipsForTexture2D(device, texture.Get())) {
    auto srv = CreateMipGenerationTexture2DSRV(device, texture.Get());
    GenerateMips(context, srv.Get());
}
```

---

## No migration required for transfer users

`D3D11TextureTransfer` remains the API for CPU readback / upload:

```cpp
ReadbackTexture2DToCpuImage(...);
CreateTexture2DFromCpuImage(...);
UpdateTexture2DFromCpuImage(...);
```

`D3D11Copy` is for GPU resource-to-resource copy, not for CPU memory transfer.

---

## Notes

`v1.6.0` does not introduce automatic hazard tracking. Callers are still responsible for using D3D11 resources in compatible ways and avoiding read/write conflicts according to D3D11 runtime rules.
