# Changelog

All notable changes to **D3D11Helper** are documented here.

This project uses semantic versioning in the following style:

- `v1.x.0`: backward-compatible feature additions and module additions
- `v1.x.y`: bug fixes, tests, and documentation-only updates
- `v2.0.0`: breaking public API changes, if they ever become necessary


---

## v1.3.0 - Presentation helpers

### Summary

`v1.3.0` expands the `D3D11Presentation` module from low-level swap-chain creation helpers into a more practical presentation layer for window rendering and offscreen render-target work.

The new helpers intentionally remain thin Direct3D 11 wrappers. They do not own the application message loop, scene renderer, UI system, frame scheduler, or high-level engine policy.

### Added

- Added offscreen render-target wrapper:
  - `D3D11RenderTargetDesc`
  - `D3D11RenderTarget`
  - `CreateRenderTarget`
  - `MakeViewport`
  - `SetViewport`
  - `UnbindRenderTargets`
- Added swap-chain wrapper with size-dependent resource management:
  - `D3D11SwapChainDesc`
  - `D3D11SwapChain`
  - `CreateSwapChain`
  - `CreateSwapChainForWindow`
- Added optional depth/stencil target support for both render-target and swap-chain paths.
- Added resize handling for swap chains:
  - `Resize(width, height)`
  - `ResizeToClientRect(hwnd)`
- Added tests for render-target creation, binding, clearing, viewport setup, swap-chain creation, resize, and invalid-argument handling.
- Added `sample/19_PresentationWindow`, a Win32 window render-loop sample that demonstrates `D3D11SwapChain`.

### Changed

- `D3D11Presentation.hpp` now includes the new presentation wrappers in addition to the existing low-level `D3D11SwapChainHelper.hpp`.
- `CMakeLists.txt` project version updated to `1.3.0`.

### Compatibility

The existing low-level APIs remain available:

```cpp
ComPtr<IDXGISwapChain3> CreateSwapChainForHwnd(...);
D3D11Resource GetSwapChainBackBuffer(IDXGISwapChain3* swapChain, UINT index);
```

New code that wants resize-safe window rendering should prefer `D3D11SwapChain`.

### Supported scope

The presentation helpers in `v1.3.0` intentionally focus on the common baseline:

- Win32 `HWND` swap chain creation
- flip-model swap chain
- color backbuffer RTV management
- optional depth/stencil target
- viewport binding
- clear / bind / present / resize
- non-MSAA render targets

### Notes on D3D11 flip-model swap chains

`D3D11SwapChain` caches only the app-facing `GetBuffer(0)` render target. Some D3D11/DXGI environments reject `GetBuffer(1+)` on flip-model swap chains with `DXGI_ERROR_INVALID_CALL`, even when `BufferCount > 1`. The wrapper therefore exposes `BufferCount()` as the requested native swap-chain buffer count, while internally managing only the current app-facing render target.

### Non-goals

`v1.3.0` does **not** add a full renderer or engine framework.

The following remain out of scope:

- scene graph
- material system
- camera system
- ImGui integration
- frame pacing policy beyond `Present(syncInterval, flags)`
- HDR color management
- MSAA resolve helper
- multi-window renderer abstraction

---

## v1.2.0 - GPU/CPU texture transfer

### Summary

`v1.2.0` adds the first transfer-oriented API set for moving data between D3D11 `Texture2D` resources and CPU memory without depending on image/video/file I/O libraries.

The purpose of this version is to provide the **Direct3D/DXGI-only primitive layer** needed by upper libraries such as `D3DTextureIO` or `D3DVideoIO`.

```text
D3D11 Texture2D
  <-> D3D11CpuImage
  <-> raw packed CPU memory
```

### Added

- Added CPU image container and row-copy helpers:
  - `D3D11CpuImage`
  - `D3D11CpuImagePlane`
  - `CreateCpuImage`
  - `ValidateCpuImage`
  - `GetPackedRowPitch`
  - `GetRequiredCpuImageSize`
  - `CopyRows`
  - `PackRows`
  - `UnpackRows`
- Added synchronous Texture2D readback:
  - `ReadbackTexture2DToCpuImage`
  - `ReadbackTexture2DRegionToCpuImage`
- Added synchronous Texture2D upload/update:
  - `CreateTexture2DFromCpuImage`
  - `UpdateTexture2DFromCpuImage`
- Added tests for CPU image layout, row-pitch utilities, readback, upload, update, and mismatch validation.
- Added `sample/18_TextureTransfer`, a file-I/O-free transfer sample.

### Supported scope

The transfer API in `v1.2.0` intentionally focuses on the safe baseline:

- `Texture2D`
- mip 0
- array slice 0
- single-plane formats
- non-block-compressed formats
- non-MSAA textures for readback
- packed or padded CPU row pitch

### Non-goals

`v1.2.0` does **not** add file/media I/O.

The following remain out of scope for D3D11Helper itself:

- PNG/JPEG/BMP/DDS load/save
- MP4/H.264/HEVC encode/decode
- Media Foundation encoder/decoder policy
- NVENC/AMF/Quick Sync/FFmpeg/OpenCV integration
- automatic format/color-space conversion
- asynchronous readback ring
- full planar format transfer

Those should be implemented by upper libraries that use `D3D11CpuImage` and the transfer APIs.

---

## v1.1.0 - Architecture normalization

### Summary

`v1.1.0` reorganizes D3D11Helper from the older 3-layer-only view into explicit public modules:

```text
D3D11Foundation
D3D11Core
D3D11Gpu
D3D11Presentation
D3D11Processing
D3D11Interop
D3D11Diagnostics
```

The goal is to keep D3D11Helper aligned with the sister project D3D12Helper at the **feature-category and module-layout level**, while still keeping each backend natural for its own API model.

### Added

- Added new public module umbrella headers:
  - `D3D11Helper/D3D11Foundation/D3D11Foundation.hpp`
  - `D3D11Helper/D3D11Gpu/D3D11Gpu.hpp`
  - `D3D11Helper/D3D11Presentation/D3D11Presentation.hpp`
  - `D3D11Helper/D3D11Interop/D3D11Interop.hpp`
  - `D3D11Helper/D3D11Diagnostics/D3D11Diagnostics.hpp`
- Added new canonical header locations for:
  - Foundation utilities
  - GPU resource/view/pipeline/shader helpers
  - Presentation helpers
  - D3D11/D3D12 interop helpers
  - Debug/diagnostics helpers
- Added architecture documentation:
  - `doc/Architecture.md`
  - `doc/D3D11Foundation.md`
  - `doc/D3D11Gpu.md`
  - `doc/D3D11Presentation.md`
  - `doc/D3D11Interop.md`
  - `doc/D3D11Diagnostics.md`
- Added module header compatibility smoke test:
  - `Test/test_module_headers.cpp`

### Changed

- `CMakeLists.txt` project version updated to `1.1.0`.
- Internal `.cpp` includes now prefer the new canonical module paths.
- Samples and tests now use the new module include paths where appropriate.
- Documentation now describes the public module layout instead of relying only on the old `Layer 1 / Layer 2 / Layer 3` model.

### Compatibility

The old include paths remain available as compatibility wrappers.

For example, this old include is still valid:

```cpp
#include <D3D11Helper/D3D11Framework/D3D11Resource.hpp>
```

The new canonical include is:

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11Resource.hpp>
```

The `D3D11Framework` module is kept as a compatibility layer for existing code.

### Non-goals

`v1.1.0` does not add image-file or video-file I/O.

D3D11Helper remains focused on Direct3D / DXGI-only primitives. File and media I/O should live in upper-level libraries such as:

```text
D3DTextureIO
D3DVideoIO
```

D3D11Helper should provide the GPU resource, readback, upload, and CPU-memory primitives needed by those libraries, but not own PNG/JPEG/MP4/NVENC/Media Foundation policy directly.

---

## v1.0.0 - Stable baseline

### Summary

Initial stable baseline before the architecture normalization work.

### Included

- `D3D11Core`
- `D3D11Framework`
- `D3D11Processing`
- Device/context/DXGI initialization
- Immediate/deferred context support
- Resource, view, sampler, staging, shader, compute, and graphics helpers
- Shared resource helpers
- D3D11.4 fence wrapper
- GPU processing helpers
