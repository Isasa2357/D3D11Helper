# Release Notes - D3D11Helper v1.7.0

## Summary

`v1.7.0` adds View / State expansion to the `D3D11Gpu` module.

This release focuses on advanced D3D11 view creation helpers and reusable state descriptor presets. It remains a Direct3D 11-native helper layer and does not introduce a material system, render graph, descriptor heap abstraction, or automatic binding framework.

## Added

### D3D11View

- Texture2D array SRV / UAV / RTV / DSV helpers
- TextureCube SRV helper
- TextureCubeArray SRV helper
- depth/stencil format mapping helpers
- depth texture DSV / SRV helpers
- typed buffer SRV / UAV helpers
- structured buffer SRV / UAV helpers
- raw buffer SRV / UAV helpers

### D3D11State

- state object creation helpers:
  - sampler state
  - rasterizer state
  - blend state
  - depth-stencil state
- `StatePresets` descriptor factories:
  - sampler presets
  - rasterizer presets
  - blend presets
  - depth-stencil presets

### Tests and samples

- Added `Test/test_view_state.cpp`.
- Added `sample/23_ViewState`.

## Changed

- `D3D11Gpu.hpp` now includes `D3D11View.hpp` and `D3D11State.hpp`.
- `CMakeLists.txt` project version is now `1.7.0`.

## Compatibility

This release is additive. Existing v1.x include paths and APIs remain available.

## Non-goals

`v1.7.0` does not add:

- graphics-stage binding set
- material system
- render graph
- automatic view cache
- descriptor heap abstraction
- shader reflection based binding
- Texture3D-specific helper layer