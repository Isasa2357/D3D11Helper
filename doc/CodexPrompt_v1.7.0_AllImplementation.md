# Codex Prompt: D3D11Helper v1.7.0 View / State Expansion

Read these files first:

```text
AGENTS.md
doc/DesignSpec_v1.7.0_ViewState.md
doc/ImplementationPlan_v1.7.0_ViewState.md
```

Implement v1.7.0 View / State Expansion in one experimental pass.

Work only on:

```text
experiment/v1.7.0-view-state-codex-all
```

Do not work on:

```text
main
feature/v1.7.0-view-state
```

## Required scope

Add advanced view helpers:

```text
include/D3D11Helper/D3D11Gpu/D3D11View.hpp
src/D3D11View.cpp
```

Add state preset helpers:

```text
include/D3D11Helper/D3D11Gpu/D3D11State.hpp
src/D3D11State.cpp
```

Add tests:

```text
Test/test_view_state.cpp
```

Add sample:

```text
sample/23_ViewState/main.cpp
```

Update only the necessary build / umbrella / test / sample registration files.

## Do not implement in this pass

Do not modify release docs or versioning:

```text
CMakeLists.txt project version
README.md release sections
CHANGELOG.md
ReleaseNotes_v1.7.0.md
MigrationGuide_v1.7.0.md
```

These are intentionally deferred.

## Commit split

Use these commits:

```text
Add D3D11 advanced view helpers
Add D3D11 state preset helpers
Add view state tests
Add view state sample
```

## Required final audit

At the end of your response, explicitly report:

```text
D3D11View and D3D11State are registered in CMakeLists.txt as sources and public headers.
D3D11View and D3D11State are included from include/D3D11Helper/D3D11Gpu/D3D11Gpu.hpp.
Test/test_view_state.cpp exists and is registered in Test/CMakeLists.txt.
sample/23_ViewState/main.cpp exists and is registered in sample/CMakeLists.txt.
CMake project version was not changed.
README.md / CHANGELOG.md / release notes / migration guide were not changed.
```

## Testing report

Run available checks and report them honestly.

If `build_and_test.cmd` cannot run because the environment is not Windows, say so clearly.

Do not claim Windows validation unless it actually ran.
