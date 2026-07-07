# ReviewChecklist v1.8.0 - D3D11Interop

## Base version

- [ ] `CMakeLists.txt` is still `VERSION 1.7.0` during implementation.
- [ ] v1.7.0 `D3D11View` / `D3D11State` files are still registered.
- [ ] No release docs or version bump are included before implementation completion.

## SharedHandle

- [ ] `D3D11SharedHandle` is move-only.
- [ ] destructor closes valid handles.
- [ ] `nullptr` and `INVALID_HANDLE_VALUE` are treated as invalid.
- [ ] `Release()` transfers ownership.
- [ ] `Reset()` closes old handle before replacing.
- [ ] `Duplicate()` uses `DuplicateHandle`.
- [ ] no raw handle leaks in tests.

## SharedTexture

- [ ] shared texture desc validates width / height / format / mip / array / sample count.
- [ ] `SharedFence` mode uses `SHARED_NTHANDLE | SHARED`.
- [ ] `KeyedMutex` mode uses `SHARED_NTHANDLE | SHARED_KEYEDMUTEX`.
- [ ] `SHARED` and `SHARED_KEYEDMUTEX` are not combined by default.
- [ ] `CreateSharedTexture2DWithHandle` returns both texture and owned handle.
- [ ] same-device open test passes.
- [ ] existing `CreateSharedTexture2D(D3D11Core&, ...)` signature mismatch is fixed.

## KeyedMutex

- [ ] attach fails on non-keyed resource.
- [ ] acquire timeout returns false only for `DXGI_ERROR_WAIT_TIMEOUT`.
- [ ] other HRESULT failures throw.
- [ ] scoped acquire releases in destructor.
- [ ] destructor is `noexcept`.
- [ ] no double release.
- [ ] tests avoid nondeterministic deadlock patterns.

## Fence

- [ ] support helpers use `QueryInterface` and safely return nullptr when unsupported.
- [ ] unsupported environment test exits successfully.
- [ ] `Initialize` / `OpenSharedHandle` clean previous state before reinitializing.
- [ ] `CpuWait(value, timeoutMs)` returns false on timeout.
- [ ] `CpuWait(value)` remains backward compatible.
- [ ] `CreateSharedHandle()` remains raw HANDLE for compatibility.
- [ ] `CreateSharedHandleOwned()` returns RAII handle.
- [ ] tests do not enqueue `GpuWait` before same-context `Signal`.

## CMake / umbrella

- [ ] new source files are registered in `CMakeLists.txt`.
- [ ] new public headers are registered in `CMakeLists.txt`.
- [ ] `D3D11Interop.hpp` includes all new headers.
- [ ] `Test/CMakeLists.txt` registers all new tests.
- [ ] `sample/CMakeLists.txt` registers `D3D11Sample_24_Interop`.
- [ ] `test_module_headers.cpp` references representative new public symbols.

## Tests

- [ ] `test_interop_shared` passes.
- [ ] `test_interop_keyed_mutex` passes.
- [ ] `test_interop_fence` passes or skip-style succeeds when unsupported.
- [ ] all existing tests still pass.
- [ ] `sample/24_Interop` builds.

## Non-goals respected

- [ ] no D3D12Helper dependency.
- [ ] no D3D11On12.
- [ ] no CUDA / Varjo / NVENC / Media Foundation.
- [ ] no external process sample.
- [ ] no file I/O.
- [ ] no automatic hazard tracking.
- [ ] no render graph.
