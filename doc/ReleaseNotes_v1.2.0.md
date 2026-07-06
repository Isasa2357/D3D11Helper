# D3D11Helper v1.2.0 Release Notes

## Overview

D3D11Helper v1.2.0 adds the first Direct3D/DXGI-only transfer layer for CPU/GPU texture movement.

The key addition is:

```text
D3D11 Texture2D
  <-> D3D11CpuImage
  <-> raw packed CPU memory
```

This is intentionally **not** image-file or video-file I/O. It is the lower-level primitive that upper libraries can use to implement PNG/JPEG/DDS/MP4/NVENC/Media Foundation workflows.

## Added

- `D3D11CpuImage`
- `D3D11CpuImagePlane`
- row pitch utilities
- synchronous Texture2D readback
- synchronous Texture2D region readback
- synchronous Texture2D creation from CPU image
- synchronous Texture2D update from CPU image
- `sample/18_TextureTransfer`
- transfer tests

## Canonical headers

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11CpuImage.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11TextureTransfer.hpp>
```

or:

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
```

## Compatibility

v1.2.0 is backward compatible with v1.1.0.

No existing public API was removed.

## Supported scope

- Texture2D
- mip 0
- array slice 0
- single-plane formats
- non-block-compressed formats
- non-MSAA readback
- packed or padded CPU row pitch

## Non-goals

D3D11Helper still does not own:

- image file I/O
- video file I/O
- encoder backend policy
- external library integration
- format/color conversion policy

Those should live in upper libraries.
