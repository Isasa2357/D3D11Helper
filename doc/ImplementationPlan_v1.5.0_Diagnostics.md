# D3D11Helper v1.5.0 Implementation Plan - Diagnostics

This plan is for Codex and human reviewers.

In this experiment, Codex may implement Steps 1 through 6 in one task, but it must keep commits separated by step.

---

## Step 0 - Guidance files

Status: human-provided setup step.

Files:

```text
AGENTS.md
doc/DesignSpec_v1.5.0_Diagnostics.md
doc/ImplementationPlan_v1.5.0_Diagnostics.md
doc/CodexPrompt_v1.5.0_AllImplementation.md
doc/CodexReviewChecklist_v1.5.0_Diagnostics.md
```

No C++ implementation belongs to Step 0.

---

## Step 1 - Device lost diagnostics

Goal: add small helpers for detecting and naming D3D11 device-lost state.

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

Required API:

```cpp
D3D11DeviceLostInfo
IsDeviceLost
DeviceLostReasonName
CheckDeviceLost
ThrowIfDeviceLost
```

Commit message:

```text
Add D3D11 device lost diagnostics helpers
```

---

## Step 2 - InfoQueue diagnostics helpers

Goal: add explicit helper functions around `ID3D11InfoQueue`.

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

Required API:

```cpp
TryGetD3D11InfoQueue
GetD3D11InfoQueue
GetInfoQueueMessageCount
ClearInfoQueueMessages
SetInfoQueueBreakOnSeverity
AddInfoQueueStorageFilterDenySeverity
```

Commit message:

```text
Add D3D11 InfoQueue diagnostics helpers
```

---

## Step 3 - GPU timestamp timer

Goal: add a small synchronous GPU timer using timestamp queries.

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

Required API:

```cpp
D3D11GpuTimerResult
D3D11GpuTimer
D3D11ScopedGpuTimer
```

Commit message:

```text
Add D3D11 GPU timestamp timer
```

---

## Step 4 - GPU profiler

Goal: add a simple sample-based GPU profiler built on timer queries.

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

Required API:

```cpp
D3D11GpuProfilerSampleResult
D3D11GpuProfiler
D3D11ScopedGpuProfileSample
```

Commit message:

```text
Add D3D11 GPU profiler
```

---

## Step 5 - Runtime tests

Goal: add diagnostics runtime tests.

Add:

```text
Test/test_diagnostics.cpp
```

Update:

```text
Test/CMakeLists.txt
```

The tests should cover:

- device lost helper classification
- normal device check
- InfoQueue helper availability and operations
- GPU timer initialize/begin/end/resolve
- GPU profiler frame/sample/resolve/reset

Do not require windows or file output.

Commit message:

```text
Add diagnostics tests
```

---

## Step 6 - Diagnostics sample

Goal: add a minimal console sample for diagnostics.

Add:

```text
sample/21_Diagnostics/main.cpp
```

Update:

```text
sample/CMakeLists.txt
```

The sample should:

- create D3D11Core
- check device lost information
- try InfoQueue
- run a GPU timer
- run a GPU profiler with one or two named samples
- print to console

No file I/O, no window, no external dependency.

Commit message:

```text
Add diagnostics sample
```

---

## Step 7 - Documentation and release notes

Status: human reviewer step.

Do not implement in the all-in-one Codex experiment.

---

## Step 8 - Guidance cleanup

Status: human reviewer step.

Do not implement in the all-in-one Codex experiment.

---

## Final validation checklist before merging to main

- `build_and_test.cmd` passes on Windows.
- No temporary `.cmd` files are tracked.
- Public headers compile through `D3D11Diagnostics.hpp`.
- Tests exist and pass.
- Sample exists and builds.
- No external dependencies were added.
- No file I/O was added.
- No Codex guidance files remain after cleanup.
