# D3D11 Generic GPU Foundation Phase 4: Fence Points

## Scope

D3D11 does not expose explicit command queues, so the D3D12 queue-sync-point API is not copied. This phase adds an interop-focused immutable `ID3D11Fence` + value object.

## API

```cpp
D3D11FencePoint point = fence.SignalPoint(context, value);
point.CpuWait();

otherFence.GpuWaitPoint(otherContext, point);
```

`D3D11FencePoint` owns a `ComPtr<ID3D11Fence>`, so the fence remains alive after the wrapper that created the point is moved or destroyed. It exposes `GetFence`, `GetValue`, `IsValid`, `IsComplete`, and `CpuWait`.

## D3D11-specific constraints

An Immediate Context has one ordered GPU timeline. A wait must not depend on a later signal queued on the same context, otherwise the GPU timeline can self-deadlock. Fence points are primarily intended for D3D11/D3D12 interop or synchronization between independently progressing contexts/devices.

## Compatibility

Existing `Signal`, `GpuWait`, and `CpuWait` methods are unchanged. New entry points use distinct names (`SignalPoint`, `GpuWaitPoint`) rather than extending the existing overloaded method sets.

## Tests

`test_interop_fence` covers default invalid points, signal/CPU-wait completion, point-owned fence lifetime, copy/move traits, and invalid argument paths. Runtime tests are skipped when D3D11.4 fence support is unavailable.
