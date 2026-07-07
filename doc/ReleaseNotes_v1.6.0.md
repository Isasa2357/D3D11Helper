# Release Notes - D3D11Helper v1.6.0

## Summary

`v1.6.0` adds Copy / Resolve / Mipmap helpers to the `D3D11Gpu` module.

The goal is to provide small D3D11-native building blocks for common GPU resource movement tasks:

- resource / subresource copy
- `Texture2D` full and region copy
- buffer full and byte-range copy
- MSAA `Texture2D` resolve
- automatic mip generation

The implementation remains DirectX / DXGI only. It does not add image/video file I/O, format conversion, async scheduling, or render graph behavior.

---

## Added headers

```text
include/D3D11Helper/D3D11Gpu/D3D11Copy.hpp
include/D3D11Helper/D3D11Gpu/D3D11Resolve.hpp
include/D3D11Helper/D3D11Gpu/D3D11Mipmap.hpp
```

These are included by:

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
```

---

## Added source files

```text
src/D3D11Copy.cpp
src/D3D11Resolve.cpp
src/D3D11Mipmap.cpp
```

---

## Added tests

```text
Test/test_copy.cpp
```

Test coverage includes:

- full `Texture2D` copy
- `Texture2D` region copy
- full buffer copy
- buffer region copy
- optional MSAA resolve
- optional mip generation
- invalid argument checks

---

## Added sample

```text
sample/22_CopyResolveMipmap
```

The sample is console-only and demonstrates:

- texture copy
- buffer copy
- optional MSAA resolve
- optional mip generation

It does not use window creation, file I/O, WIC, OpenCV, Media Foundation, or external libraries.

---

## Scope

Supported:

- full resource copy
- subresource copy
- `Texture2D` full copy
- `Texture2D` single-sample region copy
- buffer full copy
- buffer byte-range copy
- MSAA `Texture2D` resolve
- automatic mip generation through D3D11 SRV + `GenerateMips()`

Out of scope:

- CPU readback / upload beyond `D3D11TextureTransfer`
- image/video file I/O
- format conversion
- color-space conversion
- async copy scheduling
- automatic resource hazard tracking
- render graph integration
- depth/stencil resolve helper
- custom mipmap generation shader

---

## Compatibility

`v1.6.0` is intended to be backward-compatible with `v1.5.0`.

Existing include paths and APIs remain available. New code can use either the individual headers or the `D3D11Gpu.hpp` umbrella header.
