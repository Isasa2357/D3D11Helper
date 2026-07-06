# AGENTS.md - D3D11Helper Codex Instructions

This repository is **D3D11Helper**, a thin C++17 helper library for Direct3D 11.

Codex should act as an implementation worker, not as the project architect. Follow the design documents in `doc/` exactly. If something is ambiguous, choose the smallest implementation that preserves the existing public style and report the ambiguity in the summary.

---

## Project rules

- Keep the core library **DirectX/DXGI only**.
- Do not add external dependencies.
- Do not use boost, nlohmann/json, ThreadKit, WIC, Media Foundation, FFmpeg, OpenCV, NVENC, AMF, Quick Sync, or platform UI frameworks.
- Do not add file/media I/O to D3D11Helper.
- Do not implement a renderer, engine, scene graph, render graph, frame graph, GUI, or logging framework.
- Preserve v1.x backward compatibility.
- Do not remove old include paths or compatibility wrappers.
- Keep public APIs in namespace `D3D11CoreLib` unless an existing module already uses a nested namespace.
- Prefer small, explicit helper classes and free functions over large framework classes.
- Avoid unrelated formatting changes.
- Avoid large rewrites of existing modules.

---

## Current version context

The project has progressed as follows:

- v1.1.0: module layout normalization
- v1.2.0: `D3D11CpuImage` and `Texture2D` transfer
- v1.3.0: presentation helpers (`D3D11RenderTarget`, `D3D11SwapChain`)
- v1.4.0: compute binding helpers (`D3D11ComputeBindingSet`, scoped compute binding restore)
- v1.5.0: diagnostics helpers

The v1.5.0 work belongs under the existing `D3D11Diagnostics` module.

---

## Build and test

The real validation environment is Windows + Visual Studio + Direct3D 11.

Use this command on Windows:

```bat
build_and_test.cmd
```

If your environment is Linux or otherwise cannot run Windows batch files, do not pretend that the test passed. Report clearly that Windows local testing is required.

A Linux CMake configure that emits the expected “D3D11Helper only builds on Windows” warning is not a substitute for Windows testing.

---

## Temporary files

Do not commit temporary patch/apply files:

- `README_APPLY.txt`
- `apply_*.cmd`
- `build_test_and_push_if_passed.cmd`
- `finish_repair_*.cmd`
- `tools/`

Codex guidance markdown files may be committed during development if explicitly requested, but they must be removed before the final release cleanup.

---

## Commit style

For the v1.5.0 diagnostics experiment, split the implementation into small commits even when asked to implement several steps at once.

Expected commits:

1. `Add D3D11 device lost diagnostics helpers`
2. `Add D3D11 InfoQueue diagnostics helpers`
3. `Add D3D11 GPU timestamp timer`
4. `Add D3D11 GPU profiler`
5. `Add diagnostics tests and sample`

Do not include documentation/release-note cleanup unless explicitly requested.
