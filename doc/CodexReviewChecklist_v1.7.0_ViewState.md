# Codex Review Checklist: D3D11Helper v1.7.0 View / State Expansion

Use this checklist before reporting completion.

## 1. Scope

- [ ] Work was done only on `experiment/v1.7.0-view-state-codex-all`.
- [ ] No release documentation was implemented.
- [ ] `CMakeLists.txt` project version was not changed.
- [ ] `README.md` release content was not changed.
- [ ] `CHANGELOG.md` was not changed.
- [ ] No `ReleaseNotes_v1.7.0.md` or `MigrationGuide_v1.7.0.md` was added.
- [ ] No external dependencies were added.
- [ ] No file I/O, image loading, video I/O, OpenCV, FFmpeg, Media Foundation, CUDA, or NVENC code was added.

## 2. Required files

- [ ] `include/D3D11Helper/D3D11Gpu/D3D11View.hpp` exists.
- [ ] `include/D3D11Helper/D3D11Gpu/D3D11State.hpp` exists.
- [ ] `src/D3D11View.cpp` exists.
- [ ] `src/D3D11State.cpp` exists.
- [ ] `Test/test_view_state.cpp` exists.
- [ ] `sample/23_ViewState/main.cpp` exists.

## 3. Registration

- [ ] `src/D3D11View.cpp` is listed in root `CMakeLists.txt`.
- [ ] `src/D3D11State.cpp` is listed in root `CMakeLists.txt`.
- [ ] `include/D3D11Helper/D3D11Gpu/D3D11View.hpp` is listed in root `CMakeLists.txt` public headers.
- [ ] `include/D3D11Helper/D3D11Gpu/D3D11State.hpp` is listed in root `CMakeLists.txt` public headers.
- [ ] `include/D3D11Helper/D3D11Gpu/D3D11Gpu.hpp` includes both new headers.
- [ ] `Test/test_view_state.cpp` is registered in `Test/CMakeLists.txt`.
- [ ] `sample/23_ViewState/main.cpp` is registered in `sample/CMakeLists.txt`.

## 4. Public API quality

- [ ] New public headers include `D3D11Foundation/D3D11Common.hpp`, not legacy `D3D11Core/D3D11Common.hpp`.
- [ ] Public API is in `namespace D3D11CoreLib`.
- [ ] State preset functions are in a nested namespace such as `StatePresets` to avoid global name collisions.
- [ ] Existing helper names and `PipelineDefaults` were not removed or renamed.
- [ ] Public functions validate null pointers.
- [ ] Public functions validate mip, array, cube, and buffer ranges.
- [ ] HRESULT-returning D3D calls use `D3D11CORE_THROW_IF_FAILED`.

## 5. View helper coverage

- [ ] Texture2D array SRV creation is implemented.
- [ ] Texture2D array UAV creation is implemented.
- [ ] Texture2D array RTV creation is implemented.
- [ ] Texture2D array DSV creation is implemented.
- [ ] Cube SRV creation is implemented.
- [ ] Cube array SRV creation is implemented or clearly guarded.
- [ ] Depth format mapping helpers cover D32, D24S8, D16, and D32S8 families.
- [ ] Depth Texture2D DSV helper is implemented.
- [ ] Depth Texture2D SRV helper is implemented.
- [ ] Typed buffer SRV/UAV helpers are implemented.
- [ ] Structured buffer SRV/UAV helpers are implemented.
- [ ] Raw buffer SRV/UAV helpers are implemented.

## 6. State helper coverage

- [ ] Sampler state creation helper is implemented.
- [ ] Rasterizer state creation helper is implemented.
- [ ] Blend state creation helper is implemented.
- [ ] Depth-stencil state creation helper is implemented.
- [ ] Sampler presets cover point/linear clamp/wrap and anisotropic.
- [ ] Blend presets cover opaque, alpha, premultiplied alpha, and additive.
- [ ] Depth presets cover disabled, default, read-only, and reverse-Z.
- [ ] Rasterizer presets cover cull-back, cull-none, wireframe, and scissor variants.

## 7. Tests

- [ ] `test_module_headers` references new descriptor structs or preset functions.
- [ ] `test_view_state` creates Texture2D array views.
- [ ] `test_view_state` creates cube SRV.
- [ ] `test_view_state` creates depth DSV/SRV from typeless depth texture.
- [ ] `test_view_state` creates typed buffer views.
- [ ] `test_view_state` creates structured buffer views.
- [ ] `test_view_state` creates raw buffer views.
- [ ] `test_view_state` creates state objects from presets.
- [ ] Invalid argument tests exist.
- [ ] Tests do not rely on external files.

## 8. Sample

- [ ] `sample/23_ViewState/main.cpp` is console-only.
- [ ] It creates representative views.
- [ ] It creates representative state objects.
- [ ] It uses no file I/O.
- [ ] It creates no window.
- [ ] It prints `RESULT: OK` on success.

## 9. Build / formatting

- [ ] `git diff --check` passes.
- [ ] If running on Windows, `build_and_test.cmd` passes.
- [ ] If not running on Windows, report that Windows validation is still required.

## 10. Final response

The final Codex response must include:

- summary
- commits
- testing results
- final audit results
- anything deferred
