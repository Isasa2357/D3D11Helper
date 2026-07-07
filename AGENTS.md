# AGENTS.md - D3D11Helper v1.7.0 Codex Guidance

## Repository

This repository is `D3D11Helper`, a lightweight Direct3D 11 helper library.

The v1.x architecture uses these public modules:

- `D3D11Foundation`: DirectX/DXGI-only common utilities
- `D3D11Core`: device / immediate context / deferred context facade
- `D3D11Gpu`: resource, view, shader, pipeline, transfer, binding, copy helpers
- `D3D11Presentation`: swap chain / render target helpers
- `D3D11Processing`: shader-based image processing
- `D3D11Interop`: shared handles / synchronization helpers
- `D3D11Diagnostics`: debug layer / InfoQueue / GPU timer / profiler helpers

## Current task

Implement **v1.7.0 View / State expansion** in one experimental pass.

Primary focus:

- advanced view helpers for array textures, cube textures, depth views, typed/structured/raw buffer views
- state desc presets and state object creation helpers for sampler / rasterizer / blend / depth-stencil state
- runtime tests
- console-only sample

## Branch rules

Work only on:

```text
experiment/v1.7.0-view-state-codex-all
```

Do not work on:

```text
main
feature/v1.7.0-view-state
```

## Scope rules

Do not implement release documentation in this pass.

Do not change:

- `CMakeLists.txt` project version
- `CHANGELOG.md`
- `README.md` release sections
- release notes
- migration guide

Those will be handled after review and Windows validation.

Do not add external dependencies.

Allowed dependencies are the existing Windows / DirectX SDK headers and libraries already used by D3D11Helper.

Do not add:

- WIC / PNG / JPEG / DDS loading
- video I/O
- Media Foundation / FFmpeg / OpenCV
- CUDA / NVENC / vendor-specific paths
- GUI or windowing code for the new sample
- file I/O in the new sample

## Coding style

- Use C++17.
- Include `D3D11Helper/D3D11Foundation/D3D11Common.hpp` from new public module headers.
- Use `D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp` in `.cpp` files that call HRESULT-returning D3D APIs.
- Use `Microsoft::WRL::ComPtr` through the existing `D3D11CoreLib::ComPtr` alias.
- Public APIs should live in `namespace D3D11CoreLib`.
- Internal helper functions may live in anonymous namespaces inside `.cpp` files.
- Prefer free functions and lightweight descriptor structs, matching existing D3D11Helper style.
- Do not introduce heavy manager classes for v1.7.0.

## Error handling

For public helpers:

- null `ID3D11Device*`, `ID3D11DeviceContext*`, `ID3D11Resource*`, `ID3D11Texture2D*`, `ID3D11Buffer*` should throw `std::invalid_argument`
- invalid indices, ranges, element counts, mip ranges, array slice ranges, and cube ranges should throw `std::out_of_range` or `std::invalid_argument`
- HRESULT-returning D3D calls should use `D3D11CORE_THROW_IF_FAILED`
- do not silently ignore invalid calls

## Commit rules

Split commits by step:

```text
Commit 1: Add D3D11 advanced view helpers
Commit 2: Add D3D11 state preset helpers
Commit 3: Add view state tests
Commit 4: Add view state sample
```

If small fixes are needed during implementation, fold them into the relevant commit rather than creating many tiny cleanup commits.

## Required final audit

At the end, explicitly report that these files are registered in `CMakeLists.txt` and included from `D3D11Gpu.hpp`:

```text
D3D11View
D3D11State
```

Also report that these files exist:

```text
include/D3D11Helper/D3D11Gpu/D3D11View.hpp
include/D3D11Helper/D3D11Gpu/D3D11State.hpp
src/D3D11View.cpp
src/D3D11State.cpp
Test/test_view_state.cpp
sample/23_ViewState/main.cpp
```

## Testing

Run what is possible in the current environment.

If running on Linux, it is acceptable to run only syntax/diff checks and CMake configure that produces the expected non-Windows warning. Report clearly that this is not a substitute for Windows validation.

Do not claim Windows validation unless `build_and_test.cmd` actually ran on Windows.
