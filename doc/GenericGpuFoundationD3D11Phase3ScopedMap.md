# D3D11 Generic GPU Foundation Phase 3: Scoped Staging Map

## Purpose

D3D11 mapping requires an `ID3D11DeviceContext` and maps a whole subresource. Therefore the D3D12 byte-range readback API is not copied directly. This phase adds a D3D11-native move-only RAII guard around the existing staging `Map` / `Unmap` pair.

## API

```cpp
auto mapped = staging.MapScoped(context);
const std::byte* data = mapped.DataAs<std::byte>();
UINT rowPitch = mapped.RowPitch();
UINT depthPitch = mapped.DepthPitch();
```

`D3D11MappedSubresource` automatically calls `Unmap` when destroyed or reset. Move construction and move assignment transfer sole responsibility for the pending `Unmap` operation.

## Compatibility

The existing `D3D11StagingBuffer::Map()` and `Unmap()` names, signatures, and behavior are unchanged. In particular, repeated legacy `Map()` calls while already mapped still return the same pointer.

`MapScoped()` is a separately named API and rejects an already mapped staging resource. This prevents two RAII guards from attempting to unmap the same resource.

## Lifetime

The `D3D11StagingBuffer` and `ID3D11DeviceContext` must outlive the guard. The staging buffer must not be moved or destroyed while a scoped mapping is active.

## Tests

`test_staging` covers:

- legacy manual map behavior,
- buffer and texture scoped readback,
- row and depth pitch exposure,
- automatic unmap,
- move ownership transfer,
- repeated scoped-map rejection,
- remapping after guard destruction.
