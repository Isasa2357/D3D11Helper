# Migrating to D3D11Helper v1.1.0

`v1.1.0` keeps backward compatibility, so existing code should continue to build.

The migration is mostly about changing includes to the new canonical module paths.

---

## Recommended include mapping

### Foundation utilities

Old:

```cpp
#include <D3D11Helper/D3D11Core/ThrowIfFailed.hpp>
#include <D3D11Helper/D3D11Core/D3D11FormatUtil.hpp>
#include <D3D11Helper/D3D11Core/DxgiUtil.hpp>
#include <D3D11Helper/D3D11Core/DxgiAdapterSelector.hpp>
```

New:

```cpp
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>
#include <D3D11Helper/D3D11Foundation/D3D11FormatUtil.hpp>
#include <D3D11Helper/D3D11Foundation/DxgiUtil.hpp>
#include <D3D11Helper/D3D11Foundation/DxgiAdapterSelector.hpp>
```

Or:

```cpp
#include <D3D11Helper/D3D11Foundation/D3D11Foundation.hpp>
```

---

## GPU helpers

Old:

```cpp
#include <D3D11Helper/D3D11Framework/D3D11Framework.hpp>
#include <D3D11Helper/D3D11Framework/D3D11Resource.hpp>
#include <D3D11Helper/D3D11Framework/D3D11Helpers.hpp>
#include <D3D11Helper/D3D11Framework/D3D11ShaderCompiler.hpp>
```

New:

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Resource.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Helpers.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11ShaderCompiler.hpp>
```

---

## Presentation

Old:

```cpp
#include <D3D11Helper/D3D11Framework/D3D11SwapChainHelper.hpp>
```

New:

```cpp
#include <D3D11Helper/D3D11Presentation/D3D11Presentation.hpp>
```

or:

```cpp
#include <D3D11Helper/D3D11Presentation/D3D11SwapChainHelper.hpp>
```

---

## Interop

Old:

```cpp
#include <D3D11Helper/D3D11Core/D3D11Fence.hpp>
#include <D3D11Helper/D3D11Core/D3D11SharedResource.hpp>
```

New:

```cpp
#include <D3D11Helper/D3D11Interop/D3D11Fence.hpp>
#include <D3D11Helper/D3D11Interop/D3D11SharedResource.hpp>
```

Or:

```cpp
#include <D3D11Helper/D3D11Interop/D3D11Interop.hpp>
```

---

## Diagnostics

Old:

```cpp
#include <D3D11Helper/D3D11Core/D3D11Debug.hpp>
```

New:

```cpp
#include <D3D11Helper/D3D11Diagnostics/D3D11Debug.hpp>
```

Or:

```cpp
#include <D3D11Helper/D3D11Diagnostics/D3D11Diagnostics.hpp>
```

---

## Processing

Processing remains in the same public module:

```cpp
#include <D3D11Helper/D3D11Processing/D3D11Processing.hpp>
```

---

## Minimal modern include set

For a typical compute/processing application:

```cpp
#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
#include <D3D11Helper/D3D11Processing/D3D11Processing.hpp>
```

For a windowed rendering application:

```cpp
#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
#include <D3D11Helper/D3D11Presentation/D3D11Presentation.hpp>
```

For shared-resource interop:

```cpp
#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
#include <D3D11Helper/D3D11Interop/D3D11Interop.hpp>
```

---

## Notes

- The old paths remain available as compatibility wrappers.
- New code should prefer the module that matches the feature category.
- `D3D11Framework` should be considered a legacy compatibility module after `v1.1.0`.
- No file/media I/O is added in `v1.1.0`; upper-level libraries should build that on top of D3DHelper transfer/readback primitives.
