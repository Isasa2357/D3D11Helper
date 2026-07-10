# D3D11 Generic GPU Foundation Plan

This document records the D3D11-specific counterpart of the D3D12Helper v1.13.0 generic GPU foundation work.

## Compatibility baseline

- Preserve the existing D3D11Helper v1.12.0 public source surface.
- Do not remove or rename existing public types, functions, methods, return types, or header paths.
- Use separately named entry points when adding a same-name overload would make address-of expressions ambiguous.
- Preserve the existing `ColorSpace.hlsli` public functions and numeric behavior.

## D3D11-specific scope

The D3D12 APIs are not copied mechanically. D3D11 receives only features that have a natural D3D11 meaning.

### Implement

1. Reusable NV12/P010 YUV HLSL primitives with CPU/GPU golden tests.
2. Non-owning `D3D11ResourceView` and Processing `*View` dispatch paths.
3. Generic Texture2D validation.
4. Scoped staging-resource Map/Unmap RAII.
5. Optional D3D11.4 interop fence point if it proves useful after the earlier phases.

### Do not implement

- Resource barrier batches: D3D11 has no explicit D3D12-style resource-state barriers.
- Typed command allocators/lists: D3D11 uses Immediate/Deferred Contexts and `ID3D11CommandList`.
- D3D12 resource-state descriptors: D3D11 runtime manages resource transitions.
- D3D12 committed-resource descriptors: D3D11 already exposes native buffer/texture descriptors.

## Phase order

1. YUV HLSL primitives and D3D11/D3D12 golden-value parity.
2. Non-owning resource views and full Processing coverage audit.
3. Texture validation and scoped staging mapping.
4. Optional fence point, final audit, and release preparation.
