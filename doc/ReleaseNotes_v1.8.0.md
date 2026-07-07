# D3D11Helper v1.8.0 Release Notes

## Summary

`v1.8.0` expands the `D3D11Interop` module with owned shared handle helpers, shared Texture2D creation/open helpers, keyed mutex wrappers, D3D11.4 fence support checks, and hardened fence convenience overloads.

The implementation remains DirectX/DXGI-only. It does not add D3D12Helper dependency, D3D11On12 abstraction, CUDA/Varjo/NVENC integration, Media Foundation I/O, or process-level IPC management.

## Added

- Added owned HANDLE wrapper:
  - `D3D11SharedHandle`
  - move-only ownership
  - `Duplicate`
  - `Release`
  - `Reset`
- Added shared texture helpers:
  - `D3D11SharedTextureSyncMode`
  - `D3D11SharedTexture2DDesc`
  - `D3D11SharedTexture2D`
  - `MakeSharedTextureMiscFlags`
  - `ValidateSharedTexture2DDesc`
  - `CreateSharedTexture2D`
  - `CreateSharedTexture2DWithHandle`
  - `OpenSharedTexture2D`
- Added `D3D11SharedResource::CreateSharedHandleOwned`.
- Added keyed mutex helpers:
  - `GetKeyedMutex`
  - `HasKeyedMutex`
  - `D3D11KeyedMutex`
  - `D3D11ScopedKeyedMutexAcquire`
- Added D3D11.4 fence capability helpers:
  - `D3D11FenceSupportInfo`
  - `TryGetD3D11Device5`
  - `TryGetD3D11DeviceContext4`
  - `CheckD3D11FenceSupport`
  - `IsD3D11FenceSupported`
- Added `D3D11Fence` convenience overloads for `ID3D11Device*` and `ID3D11DeviceContext*`.
- Added owned fence shared handle API:
  - `D3D11Fence::CreateSharedHandleOwned`
- Added `D3D11Fence::IsInitialized`.
- Added runtime tests:
  - `Test/test_interop_shared.cpp`
  - `Test/test_interop_keyed_mutex.cpp`
  - `Test/test_interop_fence.cpp`
- Added console sample:
  - `sample/24_Interop`

## Changed

- `D3D11Interop.hpp` now includes:
  - `D3D11SharedHandle.hpp`
  - `D3D11SharedTexture.hpp`
  - `D3D11KeyedMutex.hpp`
  - `D3D11FenceSupport.hpp`
  - `D3D11Fence.hpp`
- Existing D3D11 GPU shared texture helper now delegates to the v1.8.0 shared texture path.
- `D3D11Fence::Initialize` and `OpenSharedHandle` clean up previous state before reinitialization to avoid event handle leaks.
- `D3D11Fence::CpuWait` checks the wait result instead of assuming success.
- `CMakeLists.txt` project version updated to `1.8.0`.

## Supported scope

The v1.8.0 interop baseline supports:

- NT shared handle ownership for DirectX/DXGI interop boundaries
- D3D11 shared Texture2D creation/open
- shared texture synchronization mode selection:
  - shared fence mode
  - keyed mutex mode
- `IDXGIKeyedMutex` acquire/release helper
- scoped keyed mutex acquire/release
- D3D11.4 fence support detection
- D3D11 fence signal / CPU wait / shared handle open
- runtime tests that skip fence execution when D3D11.4 fence support is unavailable

## Non-goals

The following remain out of scope for v1.8.0:

- D3D12 resource creation helpers
- D3D12 fence wrapper
- D3D11On12 abstraction
- cross-process protocol management
- named-handle registry or IPC layer
- automatic synchronization scheduling
- hazard tracking
- D3D12Helper dependency
- CUDA / NVENC / Varjo / Media Foundation / OpenCV integration
- file or video I/O

## Notes

`D3D11Fence::GpuWait` can deadlock if it waits on a value that will only be signaled later on the same immediate context. Use it for cross-device or cross-API waits where another queue/timeline will signal the value. For single D3D11 immediate-context completion, prefer `Signal + Flush + CpuWait`.
