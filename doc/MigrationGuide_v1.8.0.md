# Migration Guide: v1.7.0 to v1.8.0

`v1.8.0` is a backward-compatible feature release focused on `D3D11Interop`.

No existing public API is intentionally removed.

## Version

`CMakeLists.txt` is updated from:

```cmake
VERSION 1.7.0
```

to:

```cmake
VERSION 1.8.0
```

## Include path

New interop code should include the module umbrella:

```cpp
#include <D3D11Helper/D3D11Interop/D3D11Interop.hpp>
```

Existing compatibility paths remain available in v1.x:

```cpp
#include <D3D11Helper/D3D11Core/D3D11Fence.hpp>
#include <D3D11Helper/D3D11Core/D3D11SharedResource.hpp>
```

However, new code should prefer `D3D11Interop/` paths.

## Raw HANDLE ownership

Before v1.8.0, shared handle APIs commonly returned raw `HANDLE` values. The caller had to remember to call `CloseHandle`.

```cpp
HANDLE h = D3D11SharedResource::CreateSharedHandle(texture.Get());
// use h
CloseHandle(h);
```

In v1.8.0, prefer owned handles at ownership boundaries:

```cpp
D3D11SharedHandle h = D3D11SharedResource::CreateSharedHandleOwned(texture.Get());
// automatically closed
```

Raw `HANDLE` APIs are still available for compatibility and for APIs that require raw handles.

## Shared Texture2D creation

New code can use `D3D11SharedTexture2DDesc` instead of manually composing misc flags.

```cpp
D3D11SharedTexture2DDesc desc = {};
desc.width = 1280;
desc.height = 720;
desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
desc.bindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
desc.syncMode = D3D11SharedTextureSyncMode::SharedFence;

auto shared = CreateSharedTexture2DWithHandle(core->GetDevice(), desc);
```

Do not put sharing flags into `desc.miscFlags` directly. Use `syncMode` instead.

## Keyed mutex synchronization

For resources created with `D3D11SharedTextureSyncMode::KeyedMutex`, use `D3D11KeyedMutex` or `D3D11ScopedKeyedMutexAcquire`.

```cpp
D3D11KeyedMutex mutex(shared.Get());
mutex.AcquireOrThrow(0, 1000);
// use texture
mutex.Release(1);
```

or:

```cpp
{
    D3D11ScopedKeyedMutexAcquire scoped(mutex, 1, 0, 1000);
    // use texture
}
```

The helper does not design the key protocol for you. Producer and consumer code must agree on acquire/release key order.

## D3D11 fence support check

Fence support is environment-dependent. Check support before relying on D3D11.4 fence operations.

```cpp
auto support = CheckD3D11FenceSupport(
    core->GetDevice(),
    core->GetImmediateContext());

if (!support.supported) {
    // fall back to keyed mutex, Flush/CPU synchronization, or another path
}
```

## D3D11Fence convenience overloads

v1.8.0 adds overloads that accept base D3D11 interfaces and internally query D3D11.4 interfaces.

```cpp
D3D11Fence fence;
fence.Initialize(core->GetDevice());
fence.Signal(core->GetImmediateContext(), 1);
core->Flush();
fence.CpuWait(1);
```

The old `ID3D11Device5*` / `ID3D11DeviceContext4*` overloads remain available.

## GpuWait warning

Do not queue `GpuWait(N)` and then `Signal(N)` on the same immediate context. That can self-deadlock because commands execute in order.

Use `GpuWait` only when another device, API, or timeline will signal the waited value. For same-device CPU completion, prefer:

```cpp
fence.Signal(core->GetImmediateContext(), value);
core->Flush();
fence.CpuWait(value);
```

## Build/test

After updating, run:

```bat
build_and_test.cmd
```

The v1.8.0 test set includes shared handle/texture tests, keyed mutex tests, and fence interop tests. Fence execution tests skip unsupported environments instead of failing.
