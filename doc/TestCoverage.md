# Test Coverage Audit

This document tracks the practical test coverage of D3D11Helper as of v1.8.x.
It is not a compiler-generated line-coverage report. It is a module-by-module
coverage map used to decide which tests should be added before larger feature
work such as v1.9.0.

D3D11/D3D12 end-to-end interop tests are intentionally deferred until
D3D12Helper catches up. This table therefore focuses on D3D11Helper-only tests.

## Coverage scale

| Mark | Meaning |
| --- | --- |
| Strong | Multiple runtime tests, including normal paths and validation/error paths |
| Medium | Core paths are tested, but important variants or edge cases remain |
| Basic | Compile/smoke tests exist, but runtime behavior is only lightly checked |
| Missing | No meaningful test exists yet |
| N/A | Not applicable to the module |

## Module coverage table

| Module | Public API compile/smoke | Normal path runtime | Invalid args / validation | GPU readback correctness | Multi-device / interop within D3D11 | Samples | Status | Next focus |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| D3D11Foundation | Strong | Medium | Medium | N/A | N/A | N/A | Medium | Add more DXGI format edge cases and HRESULT text/device-lost cases |
| D3D11Core | Strong | Medium | Medium | N/A | Basic | `01_HelloDevice` | Medium | Add adapter-selection and deferred-context negative tests |
| D3D11Gpu Resource / Buffer / Texture | Strong | Medium | Medium | Medium | N/A | `04_BufferCompute`, `05_DynamicStreaming` | Medium | Add more subresource, array, mip, and row-pitch tests |
| D3D11Gpu Transfer | Strong | Strong | Medium | Strong | N/A | `18_TextureTransfer` | Strong | Add more format/row-pitch variants |
| D3D11Gpu Copy / Resolve / Mipmap | Strong | Strong | Medium | Medium | N/A | `22_CopyResolveMipmap` | Strong | Add BC/array/subresource edge cases where supported |
| D3D11Gpu View / State | Strong | Strong | Strong | N/A | N/A | `23_ViewState` | Strong | Add graphics-stage integration state tests later |
| D3D11Gpu Binding / Pipeline | Strong | Medium | Medium | Medium | N/A | `20_ComputeBindingSet` | Medium | Add graphics binding and shader-reflection-driven tests later |
| D3D11Presentation | Strong | Medium | Medium | Medium | N/A | `19_PresentationWindow` | Medium | Add resize/backbuffer recreation stress test |
| D3D11Diagnostics | Strong | Medium | Medium | N/A | N/A | `21_Diagnostics` | Medium | Add InfoQueue filtering and profiler misuse cases |
| D3D11Interop SharedHandle / SharedTexture | Strong | Strong | Medium | Medium | Medium | `24_Interop` | Strong | Add more access-right/name/duplicate-handle tests |
| D3D11Interop KeyedMutex | Strong | Strong | Strong | N/A | Medium | `24_Interop` | Strong | Add timeout and two-wrapper contention behavior tests |
| D3D11Interop Fence | Strong | Medium | Medium | N/A | Medium | `24_Interop` | Medium | Add additional supported-hardware fence ordering tests |
| D3D11Processing FormatConvert | Strong | Medium | Medium | Medium | N/A | `07`, `08` | Medium | Add more golden pixel tests for matrix/range/planar cases |
| D3D11Processing Resize | Strong | Medium | Basic | Medium | N/A | `07` | Medium | Add point/linear golden pixels, rect tests, odd sizes |
| D3D11Processing Remap | Strong | Medium | Basic | Medium | N/A | N/A | Medium | Add identity/shift/border/normalized coordinate golden tests |
| D3D11Processing Composite | Strong | Medium | Basic | Medium | N/A | N/A | Medium | Add blend-mode matrix and opacity golden tests |
| D3D11Processing Blur / RegionBlur | Strong | Medium | Basic | Basic | N/A | `09`, `11`, `17` | Basic | Add tolerance-based kernel/region golden tests |
| D3D11Processing RegionEffect | Strong | Medium | Basic | Medium | N/A | `10` | Medium | Add mask boundary, edge softness, and effect-mode golden tests |
| D3D11Processing ColorAdjust / Kernel / Mask / Threshold / Pyramid | Strong | Medium | Basic | Medium | N/A | `12`-`17` | Medium | Add more pixel-level reference outputs |

## Current priorities before v1.9.0

1. Keep the broad CTest suite passing in Debug.
2. Add a Release build test path and run it before release tags.
3. Expand Processing golden pixel tests because v1.9.0 is likely to touch Processing.
4. Keep D3D12-dependent tests out of this repository until D3D12Helper is ready.

## Notes

- Tests that depend on optional hardware capabilities should skip cleanly rather
  than fail on unsupported adapters.
- Pixel tests should prefer small textures with exact or near-exact expected
  output. Use tolerances only where interpolation, planar format conversion, or
  UNORM rounding makes exact byte equality unrealistic.
- New public APIs should be represented in `test_module_headers.cpp` and, when
  possible, in at least one runtime CTest target.
