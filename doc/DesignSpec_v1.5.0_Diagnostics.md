# D3D11Helper v1.5.0 Design Spec - Diagnostics

This document defines the v1.5.0 diagnostics feature set. Codex must follow this spec and avoid redesigning the architecture.

---

## Goal

v1.5.0 adds diagnostics helpers to D3D11Helper:

- device lost diagnostics
- InfoQueue helpers
- GPU timestamp timer
- simple GPU profiler
- runtime tests
- console sample

The implementation should be small, synchronous, explicit, and DirectX/DXGI-only.

---

## Module

All new public diagnostics APIs belong under:

```text
include/D3D11Helper/D3D11Diagnostics/
src/
```

The diagnostics umbrella must include the new headers:

```cpp
#include <D3D11Helper/D3D11Diagnostics/D3D11Diagnostics.hpp>
```

Do not put diagnostics APIs under `D3D11Gpu`, `D3D11Framework`, or `D3D11Processing`.

---

## Non-goals

Do **not** implement the following in v1.5.0:

- async multi-frame profiler ring
- Chrome trace output
- CSV/JSON/log file output
- GUI profiler
- ImGui integration
- renderer or frame graph
- automatic instrumentation
- shader reflection
- command-list abstraction
- multi-threaded profiler aggregation
- D3D12-specific diagnostics

File I/O is explicitly out of scope.

---

## Device lost diagnostics

### Files

Add:

```text
include/D3D11Helper/D3D11Diagnostics/D3D11DeviceLost.hpp
src/D3D11DeviceLost.cpp
```

Update:

```text
include/D3D11Helper/D3D11Diagnostics/D3D11Diagnostics.hpp
CMakeLists.txt
Test/test_module_headers.cpp
```

### Public API

```cpp
struct D3D11DeviceLostInfo {
    HRESULT reason = S_OK;
    bool deviceLost = false;
    const char* name = "S_OK";
};

bool IsDeviceLost(HRESULT hr) noexcept;
const char* DeviceLostReasonName(HRESULT hr) noexcept;
D3D11DeviceLostInfo CheckDeviceLost(ID3D11Device* device);
void ThrowIfDeviceLost(ID3D11Device* device);
```

### Behavior

- `IsDeviceLost(S_OK)` returns false.
- Device-lost reasons return true:
  - `DXGI_ERROR_DEVICE_REMOVED`
  - `DXGI_ERROR_DEVICE_RESET`
  - `DXGI_ERROR_DRIVER_INTERNAL_ERROR`
  - `DXGI_ERROR_INVALID_CALL`
- `DeviceLostReasonName` returns stable string literals.
- `CheckDeviceLost(nullptr)` should throw `std::invalid_argument`.
- `CheckDeviceLost(device)` calls `device->GetDeviceRemovedReason()`.
- `ThrowIfDeviceLost(device)` throws when `CheckDeviceLost(device).deviceLost` is true.

Do not call `ExitProcess`, show message boxes, recreate the device, or own application recovery policy.

---

## InfoQueue helpers

### Files

Add:

```text
include/D3D11Helper/D3D11Diagnostics/D3D11InfoQueue.hpp
src/D3D11InfoQueue.cpp
```

Update:

```text
include/D3D11Helper/D3D11Diagnostics/D3D11Diagnostics.hpp
CMakeLists.txt
```

### Public API

```cpp
ComPtr<ID3D11InfoQueue> TryGetD3D11InfoQueue(ID3D11Device* device);
ComPtr<ID3D11InfoQueue> GetD3D11InfoQueue(ID3D11Device* device);

UINT64 GetInfoQueueMessageCount(ID3D11InfoQueue* queue);
void ClearInfoQueueMessages(ID3D11InfoQueue* queue);

void SetInfoQueueBreakOnSeverity(
    ID3D11InfoQueue* queue,
    D3D11_MESSAGE_SEVERITY severity,
    bool enable);

void AddInfoQueueStorageFilterDenySeverity(
    ID3D11InfoQueue* queue,
    const D3D11_MESSAGE_SEVERITY* severities,
    UINT severityCount);
```

### Behavior

- `TryGetD3D11InfoQueue(nullptr)` returns `nullptr` or throws? Use the project’s explicit validation style: throw `std::invalid_argument` for null device.
- `TryGetD3D11InfoQueue(device)` returns `nullptr` if the debug layer / InfoQueue interface is unavailable.
- `GetD3D11InfoQueue(device)` throws `std::runtime_error` if unavailable.
- Queue helper functions throw `std::invalid_argument` on null queue.
- `severityCount == 0` is a no-op for filter additions.
- If `severityCount != 0`, the severity array must not be null.

Do not print messages, dump logs, or add file output in v1.5.0.

---

## GPU timer

### Files

Add:

```text
include/D3D11Helper/D3D11Diagnostics/D3D11GpuTimer.hpp
src/D3D11GpuTimer.cpp
```

Update:

```text
include/D3D11Helper/D3D11Diagnostics/D3D11Diagnostics.hpp
CMakeLists.txt
```

### Public API

```cpp
struct D3D11GpuTimerResult {
    bool completed = false;
    bool valid = false;
    bool disjoint = false;

    UINT64 startTimestamp = 0;
    UINT64 endTimestamp = 0;
    UINT64 frequency = 0;

    double seconds = 0.0;
    double milliseconds = 0.0;
};

class D3D11GpuTimer {
public:
    void Initialize(ID3D11Device* device);
    void Reset() noexcept;

    bool IsInitialized() const noexcept;
    bool IsRunning() const noexcept;
    bool HasEnded() const noexcept;

    void Begin(ID3D11DeviceContext* context);
    void End(ID3D11DeviceContext* context);

    D3D11GpuTimerResult Resolve(ID3D11DeviceContext* context, bool wait = true);
};

class D3D11ScopedGpuTimer {
public:
    D3D11ScopedGpuTimer(D3D11GpuTimer& timer, ID3D11DeviceContext* context);
    ~D3D11ScopedGpuTimer() noexcept;

    D3D11ScopedGpuTimer(const D3D11ScopedGpuTimer&) = delete;
    D3D11ScopedGpuTimer& operator=(const D3D11ScopedGpuTimer&) = delete;
};
```

### Internal queries

Use D3D11 timestamp queries:

```text
D3D11_QUERY_TIMESTAMP_DISJOINT
D3D11_QUERY_TIMESTAMP
D3D11_QUERY_TIMESTAMP
```

### Behavior

- `Initialize(nullptr)` throws.
- `Begin(nullptr)` / `End(nullptr)` / `Resolve(nullptr)` throw.
- `Begin()` requires initialized and not already running.
- `End()` requires running.
- `Resolve(wait=false)` returns `completed=false` if results are not ready.
- `Resolve(wait=true)` may spin/yield until results are ready.
- Do not assume `milliseconds > 0` in tests.
- If disjoint is true, `valid=false`.
- If completed and not disjoint and frequency != 0, compute seconds/milliseconds.

Do not use file output or global profiler state.

---

## GPU profiler

### Files

Add:

```text
include/D3D11Helper/D3D11Diagnostics/D3D11GpuProfiler.hpp
src/D3D11GpuProfiler.cpp
```

Update:

```text
include/D3D11Helper/D3D11Diagnostics/D3D11Diagnostics.hpp
CMakeLists.txt
```

### Public API

```cpp
struct D3D11GpuProfilerSampleResult {
    std::string name;
    bool completed = false;
    bool valid = false;
    bool disjoint = false;
    double milliseconds = 0.0;
};

class D3D11GpuProfiler {
public:
    void Initialize(ID3D11Device* device);
    void Reset();

    bool IsInitialized() const noexcept;

    void BeginFrame(ID3D11DeviceContext* context);
    void EndFrame(ID3D11DeviceContext* context);

    UINT BeginSample(ID3D11DeviceContext* context, std::string name);
    void EndSample(ID3D11DeviceContext* context, UINT sampleId);

    std::vector<D3D11GpuProfilerSampleResult> Resolve(
        ID3D11DeviceContext* context,
        bool wait = true);
};

class D3D11ScopedGpuProfileSample {
public:
    D3D11ScopedGpuProfileSample(
        D3D11GpuProfiler& profiler,
        ID3D11DeviceContext* context,
        std::string name);

    ~D3D11ScopedGpuProfileSample() noexcept;

    D3D11ScopedGpuProfileSample(const D3D11ScopedGpuProfileSample&) = delete;
    D3D11ScopedGpuProfileSample& operator=(const D3D11ScopedGpuProfileSample&) = delete;
};
```

### Behavior

- Use `D3D11GpuTimer` internally for samples unless a better small implementation is necessary.
- v1.5.0 supports a simple single-frame sample list.
- `BeginFrame` clears previous sample state and marks a frame active.
- `EndFrame` marks frame inactive.
- `BeginSample` requires an active frame.
- `EndSample` requires a valid sample id that has not already ended.
- Nested samples are allowed only if the implementation remains simple; nesting semantics do not need special tree output in v1.5.0.
- `Resolve` returns one result per sample.
- Do not implement async multi-frame ring.
- Do not output trace files.

---

## Tests

Add:

```text
Test/test_diagnostics.cpp
```

Update:

```text
Test/CMakeLists.txt
```

Test with WARP-compatible behavior. Avoid requiring real hardware timing precision.

Required coverage:

### Device lost

- `IsDeviceLost(S_OK)` false.
- representative device lost HRESULTs true.
- `DeviceLostReasonName` returns non-empty names.
- normal device check returns a valid info struct.

### InfoQueue

- With debug layer enabled, try to get InfoQueue.
- If unavailable, the test should skip that subtest safely instead of failing.
- If available, call message count, clear, break severity, filter helpers.

### GPU timer

- Initialize.
- Begin/End.
- Flush.
- Resolve(wait=true).
- Result is completed or safely disjoint-handled.
- Do not require `milliseconds > 0`.

### GPU profiler

- Initialize.
- BeginFrame.
- BeginSample / EndSample for at least two samples.
- EndFrame.
- Resolve(wait=true).
- Result count matches sample count.
- Reset is safe.

Do not create windows. Do not write files.

---

## Sample

Add:

```text
sample/21_Diagnostics/main.cpp
```

Update:

```text
sample/CMakeLists.txt
```

Sample behavior:

- Create `D3D11Core`.
- Print device lost check result.
- Try InfoQueue.
- Run a simple GPU timer around an empty GPU section.
- Run a GPU profiler frame with one or two named samples.
- Print results to console.

No file I/O, no window, no external dependency.

---

## Documentation and cleanup are out of scope for Codex in this task

Codex must not implement release docs unless explicitly asked.

Do not add:

- `doc/ReleaseNotes_v1.5.0.md`
- `doc/MigrationGuide_v1.5.0.md`
- `doc/D3D11GpuTimer.md`
- `doc/D3D11GpuProfiler.md`

Those will be written by the reviewer after implementation is accepted.
