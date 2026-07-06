# D3D11Helper v1.1.0 Release Notes

## Purpose

`v1.1.0` is an architecture-normalization release.

The main purpose is to make D3D11Helper easier to grow alongside D3D12Helper by using the same high-level feature categories:

```text
Foundation
Core
Gpu
Presentation
Processing
Interop
Diagnostics
```

This release does not try to hide D3D11 and D3D12 differences. Instead, it aligns the public module structure and documentation while keeping the implementation natural for Direct3D 11.

---

## New public module layout

```text
include/D3D11Helper/
├── D3D11Foundation/
├── D3D11Core/
├── D3D11Gpu/
├── D3D11Presentation/
├── D3D11Processing/
├── D3D11Interop/
├── D3D11Diagnostics/
└── D3D11Framework/      # compatibility layer
```

### D3D11Foundation

Low-level utilities that do not own a D3D11 device or context.

Examples:

```cpp
#include <D3D11Helper/D3D11Foundation/D3D11Foundation.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>
#include <D3D11Helper/D3D11Foundation/D3D11FormatUtil.hpp>
```

### D3D11Core

Device, immediate context, deferred context, DXGI factory/adapter, and the main `D3D11Core` facade.

Example:

```cpp
#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
```

### D3D11Gpu

Resource, view, sampler, staging, shader, compute, and graphics helpers.

Example:

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
```

### D3D11Presentation

Window/swap-chain presentation helpers.

Example:

```cpp
#include <D3D11Helper/D3D11Presentation/D3D11Presentation.hpp>
```

### D3D11Processing

GPU image-processing layer.

Example:

```cpp
#include <D3D11Helper/D3D11Processing/D3D11Processing.hpp>
```

### D3D11Interop

D3D11/D3D12 and external-API interop primitives that remain DirectX/DXGI based.

Example:

```cpp
#include <D3D11Helper/D3D11Interop/D3D11Interop.hpp>
```

### D3D11Diagnostics

Debug layer, InfoQueue, live-object reporting, and future profiling/diagnostic helpers.

Example:

```cpp
#include <D3D11Helper/D3D11Diagnostics/D3D11Diagnostics.hpp>
```

---

## Compatibility policy

Existing code using the old include paths should continue to build.

Old include:

```cpp
#include <D3D11Helper/D3D11Framework/D3D11Framework.hpp>
```

Preferred new include:

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
```

Old include:

```cpp
#include <D3D11Helper/D3D11Core/D3D11Fence.hpp>
```

Preferred new include:

```cpp
#include <D3D11Helper/D3D11Interop/D3D11Fence.hpp>
```

Old include:

```cpp
#include <D3D11Helper/D3D11Core/D3D11Debug.hpp>
```

Preferred new include:

```cpp
#include <D3D11Helper/D3D11Diagnostics/D3D11Debug.hpp>
```

`D3D11Framework` is now treated as a compatibility module. New code should prefer `D3D11Gpu`, `D3D11Presentation`, `D3D11Interop`, and `D3D11Diagnostics` as appropriate.

---

## What did not change

- Namespace remains `D3D11CoreLib`.
- Processing namespace remains `D3D11CoreLib::Processing`.
- Existing public classes and free functions remain available.
- D3D11Helper remains a Direct3D/DXGI-focused helper library.
- File and video I/O are intentionally not added to the core library.

---

## Validation

`v1.1.0` adds a module-header smoke test so that the following are checked by CTest:

- New umbrella headers can be included.
- Old compatibility wrapper headers can still be included.
- Representative public symbols are visible through the intended headers.

Run:

```bat
build_and_test.cmd
```

or manually:

```bat
cmake -S . -B out/build/default -DD3D11HELPER_BUILD_SAMPLES=ON -DD3D11HELPER_BUILD_TESTS=ON
cmake --build out/build/default --config Debug
ctest --test-dir out/build/default -C Debug --output-on-failure
```
