# Codex Review Checklist - v1.5.0 Diagnostics

Use this checklist before accepting Codex output.

---

## Scope

- [ ] New APIs are under `D3D11Diagnostics`.
- [ ] No APIs were incorrectly added to `D3D11Gpu` or `D3D11Framework`.
- [ ] No external dependencies were added.
- [ ] No file I/O was added.
- [ ] No window/UI code was added to diagnostics tests or sample.
- [ ] No unrelated formatting changes.

---

## Device lost

- [ ] `D3D11DeviceLost.hpp/.cpp` exist.
- [ ] `IsDeviceLost` handles representative DXGI device-lost HRESULT values.
- [ ] `DeviceLostReasonName` returns stable string literals.
- [ ] `CheckDeviceLost` validates null device.
- [ ] `ThrowIfDeviceLost` does not own application recovery policy.

---

## InfoQueue

- [ ] `D3D11InfoQueue.hpp/.cpp` exist.
- [ ] `TryGetD3D11InfoQueue` is safe when debug layer / InfoQueue is unavailable.
- [ ] `GetD3D11InfoQueue` throws if unavailable.
- [ ] Queue helper functions validate null queue.
- [ ] Severity filter helper handles zero-count no-op.

---

## GPU timer

- [ ] `D3D11GpuTimer.hpp/.cpp` exist.
- [ ] Uses timestamp disjoint + timestamp queries.
- [ ] `Begin` / `End` validate state transitions.
- [ ] `Resolve(wait=false)` can return incomplete.
- [ ] `Resolve(wait=true)` handles wait safely.
- [ ] Disjoint result is not reported as valid.
- [ ] Tests do not require non-zero milliseconds.

---

## GPU profiler

- [ ] `D3D11GpuProfiler.hpp/.cpp` exist.
- [ ] Uses simple sample list, not an async ring.
- [ ] `BeginFrame` / `EndFrame` state is validated.
- [ ] `BeginSample` / `EndSample` state is validated.
- [ ] `Resolve` returns one result per sample.
- [ ] Scoped sample destructor is noexcept.

---

## Tests

- [ ] `Test/test_diagnostics.cpp` exists.
- [ ] `Test/CMakeLists.txt` registers the test.
- [ ] Tests are WARP-compatible.
- [ ] InfoQueue tests safely skip if InfoQueue is unavailable.
- [ ] GPU timer/profiler tests do not compare exact timing values.

---

## Sample

- [ ] `sample/21_Diagnostics/main.cpp` exists.
- [ ] `sample/CMakeLists.txt` registers the sample.
- [ ] Sample prints to console only.
- [ ] Sample does not write files.
- [ ] Sample does not create a window.

---

## Build hygiene

- [ ] `CMakeLists.txt` includes all new `.cpp` and public headers.
- [ ] `D3D11Diagnostics.hpp` includes all new public headers.
- [ ] `test_module_headers.cpp` includes smoke coverage.
- [ ] No temporary `.cmd`, `README_APPLY.txt`, or `tools/` are tracked.
- [ ] `build_and_test.cmd` passes on Windows before merge.
