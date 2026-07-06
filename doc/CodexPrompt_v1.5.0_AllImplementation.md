# Codex Prompt - D3D11Helper v1.5.0 Diagnostics All Implementation Experiment

Read these files first:

- `AGENTS.md`
- `doc/DesignSpec_v1.5.0_Diagnostics.md`
- `doc/ImplementationPlan_v1.5.0_Diagnostics.md`
- `doc/CodexReviewChecklist_v1.5.0_Diagnostics.md`

You are implementing an **experimental all-in-one v1.5.0 diagnostics pass** on the experiment branch:

```text
experiment/v1.5.0-diagnostics-codex-all
```

Do not work on `main`.
Do not work on `feature/v1.5.0-diagnostics` unless explicitly instructed.

---

## Task

Implement v1.5.0 Steps 1 through 6 from `doc/ImplementationPlan_v1.5.0_Diagnostics.md`.

That includes:

1. Device lost diagnostics helpers
2. InfoQueue diagnostics helpers
3. GPU timestamp timer
4. GPU profiler
5. diagnostics runtime tests
6. diagnostics console sample

Do **not** implement docs/release notes/cleanup.

---

## Strict scope

Follow `doc/DesignSpec_v1.5.0_Diagnostics.md` exactly.

Do not add features outside the spec.
Do not redesign existing modules.
Do not change public APIs from previous versions.
Do not remove compatibility headers.
Do not add external dependencies.
Do not add file I/O.
Do not add GUI/window code.
Do not add CSV/JSON/trace output.
Do not add large async profiler systems.

---

## Expected files

Likely files to add:

```text
include/D3D11Helper/D3D11Diagnostics/D3D11DeviceLost.hpp
include/D3D11Helper/D3D11Diagnostics/D3D11InfoQueue.hpp
include/D3D11Helper/D3D11Diagnostics/D3D11GpuTimer.hpp
include/D3D11Helper/D3D11Diagnostics/D3D11GpuProfiler.hpp
src/D3D11DeviceLost.cpp
src/D3D11InfoQueue.cpp
src/D3D11GpuTimer.cpp
src/D3D11GpuProfiler.cpp
Test/test_diagnostics.cpp
sample/21_Diagnostics/main.cpp
```

Likely files to update:

```text
CMakeLists.txt
include/D3D11Helper/D3D11Diagnostics/D3D11Diagnostics.hpp
Test/CMakeLists.txt
Test/test_module_headers.cpp
sample/CMakeLists.txt
```

Do not add:

```text
README_APPLY.txt
apply_*.cmd
build_test_and_push_if_passed.cmd
tools/
```

---

## Commit splitting requirement

Even though this is one task, split commits by step:

```text
Add D3D11 device lost diagnostics helpers
Add D3D11 InfoQueue diagnostics helpers
Add D3D11 GPU timestamp timer
Add D3D11 GPU profiler
Add diagnostics tests
Add diagnostics sample
```

If your environment cannot create commits, provide a full unified diff and clearly state that it was not pushed.

---

## Testing

Run what you can:

```text
git diff --check
```

On Windows, the required validation is:

```bat
build_and_test.cmd
```

If your environment cannot run `cmd.exe` or Visual Studio/MSVC, say clearly:

```text
Windows local testing is required.
```

Do not claim that build/test passed unless `build_and_test.cmd` actually ran successfully on Windows.

---

## Summary expected after implementation

Your final summary must include:

- files added
- files updated
- commit hashes, if commits were created
- exact tests run
- whether Windows local testing is still required
- any intentionally deferred items
