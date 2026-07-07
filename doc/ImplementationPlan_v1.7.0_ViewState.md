# Implementation Plan: D3D11Helper v1.7.0 View / State Expansion

This plan is for Codex implementation on the experimental branch only.

## Branch

Use only:

```text
experiment/v1.7.0-view-state-codex-all
```

Do not modify `main` or `feature/v1.7.0-view-state`.

## Step 0: Read context

Read:

```text
AGENTS.md
README.md
CHANGELOG.md
CMakeLists.txt
include/D3D11Helper/D3D11Gpu/D3D11Gpu.hpp
include/D3D11Helper/D3D11Gpu/D3D11Helpers.hpp
include/D3D11Helper/D3D11Gpu/D3D11GraphicsPipeline.hpp
include/D3D11Helper/D3D11Foundation/D3D11FormatUtil.hpp
Test/CMakeLists.txt
sample/CMakeLists.txt
```

Confirm existing basic view helpers and state defaults before adding new APIs.

## Step 1: Add D3D11View helpers

Add:

```text
include/D3D11Helper/D3D11Gpu/D3D11View.hpp
src/D3D11View.cpp
```

Implement:

- Texture2D array SRV/UAV/RTV/DSV helpers
- cube texture SRV helper
- cube array SRV helper
- depth format mapping helpers
- depth Texture2D DSV/SRV helpers
- typed buffer SRV/UAV helpers
- structured buffer SRV/UAV helpers
- raw buffer SRV/UAV helpers

Validation requirements:

- null pointer checks
- mip range checks
- array slice checks
- cube range checks
- buffer element count checks
- raw buffer flag checks
- structured buffer flag/stride checks
- format defaulting and format mapping checks

Add root CMake and `D3D11Gpu.hpp` registration as part of this step.

Commit:

```text
Add D3D11 advanced view helpers
```

## Step 2: Add D3D11State helpers

Add:

```text
include/D3D11Helper/D3D11Gpu/D3D11State.hpp
src/D3D11State.cpp
```

Implement:

- `CreateSamplerState`
- `CreateRasterizerState`
- `CreateBlendState`
- `CreateDepthStencilState`
- `StatePresets` descriptor factory functions for sampler, rasterizer, blend, depth-stencil state

Do not remove or rename existing `PipelineDefaults` or old sampler helpers.

Add root CMake and `D3D11Gpu.hpp` registration as part of this step.

Commit:

```text
Add D3D11 state preset helpers
```

## Step 3: Add tests

Add:

```text
Test/test_view_state.cpp
```

Update:

```text
Test/CMakeLists.txt
Test/test_module_headers.cpp
```

Test coverage:

- module header smoke test for new descriptor structs
- Texture2D array view creation
- cube SRV creation
- depth DSV/SRV creation with typeless depth texture
- structured buffer SRV/UAV creation
- raw buffer SRV/UAV creation
- typed buffer SRV/UAV creation
- state object creation from presets
- invalid argument checks

The test should not rely on external files.

Commit:

```text
Add view state tests
```

## Step 4: Add sample

Add:

```text
sample/23_ViewState/main.cpp
```

Update:

```text
sample/CMakeLists.txt
```

Sample requirements:

- console-only
- no file I/O
- no window
- no external dependencies
- no shader compilation requirement
- create a minimal `D3D11Core`
- create representative views and state objects
- print concise status lines
- print `RESULT: OK` on success

Commit:

```text
Add view state sample
```

## Step 5: Final audit

Before final response, verify:

```text
find D3D11View / D3D11State in CMakeLists.txt source list
find D3D11View / D3D11State in CMakeLists.txt public header list
find D3D11View / D3D11State includes in include/D3D11Helper/D3D11Gpu/D3D11Gpu.hpp
confirm Test/test_view_state.cpp exists and is registered
confirm sample/23_ViewState/main.cpp exists and is registered
confirm README.md / CHANGELOG.md / release notes / migration guide were not changed
confirm CMake project version was not changed
```

Report this audit in the final Codex summary.

## Expected changed files

```text
CMakeLists.txt
include/D3D11Helper/D3D11Gpu/D3D11Gpu.hpp
include/D3D11Helper/D3D11Gpu/D3D11View.hpp
include/D3D11Helper/D3D11Gpu/D3D11State.hpp
src/D3D11View.cpp
src/D3D11State.cpp
Test/CMakeLists.txt
Test/test_module_headers.cpp
Test/test_view_state.cpp
sample/CMakeLists.txt
sample/23_ViewState/main.cpp
```

No docs/release files should be changed in this Codex pass.

## Testing

Run:

```text
git diff --check HEAD~4..HEAD
```

If on Windows, run:

```text
build_and_test.cmd
```

If not on Windows, do not claim Windows validation. A Linux CMake configure that exits with the expected non-Windows warning is useful but not sufficient.
