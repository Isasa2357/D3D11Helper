# D3D11Helper v1.12.0 Release Notes

## Theme

Advanced Processing parity with D3D12Helper.

This release adds the D3D11 side of the v1.12.0 Processing expansion with explicit matrix-based transform passes, 3D LUT application, and an undistort-map application facade.

## Added

- Added `D3D11AdvancedProcessor`.
- Added affine transform API:
  - `AffineTransformDesc`
  - `D3D11AdvancedProcessor::DispatchAffineTransform`
- Added perspective / homography transform API:
  - `PerspectiveTransformDesc`
  - `D3D11AdvancedProcessor::DispatchPerspectiveTransform`
- Added 3D LUT API:
  - `Lut3DDesc`
  - `D3D11AdvancedProcessor::DispatchApplyLut3D`
- Added undistort-map application API:
  - `D3D11AdvancedProcessor::DispatchApplyUndistortMap`
  - This is a thin facade over the existing `D3D11Remapper` path and uses `RemapDesc` / `R32G32_FLOAT` maps.
- Added compute shaders:
  - `shaders/D3D11Processing/AdvancedTransformRgba.hlsl`
  - `shaders/D3D11Processing/ApplyLut3D.hlsl`
- Added test suite:
  - `AdvancedProcessing`
  - Shader compilation tests
  - Public default tests
  - Initialization / output texture creation tests
  - Golden runtime tests for affine identity, perspective identity, and 3D LUT identity

## Scope

The current implementation supports RGBA-like source and destination textures. Transform coordinates are destination-local pixel coordinates mapped to source-local pixel coordinates. `DispatchApplyLut3D` expects a `Texture3D` LUT with RGB domain `[0, 1]` and performs manual trilinear interpolation.

## Notes

The API mirrors the D3D12Helper v1.12.0 `D3D12AdvancedProcessor` shape, while keeping D3D11 naming conventions: GPU work is exposed as `Dispatch*` instead of `Record*` because D3D11 has no explicit command-list recording layer in this helper.

## Non-goals for this first v1.12.0 step

- Exhaustive transform coverage such as rotation / scale / arbitrary homography golden cases.
- 1D LUT / 2D strip LUT variants.
- Automatic generation of undistort maps from camera calibration parameters.
