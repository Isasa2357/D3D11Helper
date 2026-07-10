# D3D11 Generic GPU Foundation Phase 1: YUV HLSL Primitives

Phase 1 adds a reusable, format-aware HLSL function library for application-owned fused shaders and aligns the D3D11 color-conversion behavior with the D3D12Helper v1.13.0 golden values.

## Files

- `shaders/D3D11Processing/YuvPrimitives.hlsli`
  - NV12/P010 sample and video-code conversion
  - BT.601, BT.709, and BT.2020
  - Full and Limited range
  - Point and linear YUV420 resize sampling
  - Point and linear logical-RGBA resize sampling
  - YUV420 luma/chroma plane stores
- `shaders/D3D11Processing/YuvPrimitiveProbe.hlsl`
  - GPU golden-value probe
- `Test/test_yuv_hlsl_primitives.cpp`
  - CPU reference values, shader compilation, GPU probe, and actual plane-storage readback

## P010 storage

A P010 texture plane is exposed through `R16_UNORM` or `R16G16_UNORM`, while the 10-bit video code occupies bits 15:6 of the 16-bit word.

```text
10-bit code <-> (code << 6) / 65535
```

`D3D11YuvSampleToCode` and `D3D11YuvCodeToSample` implement this mapping explicitly.

## Built-in shader integration

The following built-in shaders now use the shared primitives:

- `ConvertYuv420ToRgb.hlsl`
- `ConvertRgbToYuv420.hlsl`
- `FusedYuv420ToRgbResize.hlsl`
- `ResizeRgba.hlsl`
- `FusedRgbToRgbResize.hlsl` through `ResizeRgba.hlsl`
- `sample/25_ProcessingCustomFusedShader/CustomNv12ResizeDarken.hlsl`

Existing shader filenames, C++ public APIs, constant-buffer layout, and resource slots remain unchanged.

## Compatibility

`ColorSpace.hlsli` is intentionally unchanged. Existing application shaders that call `DecodeYuv` or `EncodeYuv` retain their previous numeric behavior. New format-aware shaders should include `YuvPrimitives.hlsli` and pass `SrcFormat` or `DstFormat` explicitly.

The first Windows validation run exposed that `matrix` is parsed as a type keyword by the D3DCompile / Shader Model 5 path. Function parameter names were changed to `colorMatrix`; function names, argument order, and numeric behavior were not changed.

## Tests

The phase test covers:

- known limited-range red codes for BT.601/709/2020,
- NV12 and P010 CPU encode/decode round trips,
- P010 high-bit storage round trips,
- compilation of every refactored built-in shader,
- GPU probe output compared with CPU reference values,
- actual RGBA red to NV12 plane bytes,
- actual RGBA red to P010 high-bit plane words.

NV12/P010 storage tests are skipped on devices that do not expose the required planar UAV support.

After the D3DCompile compatibility correction, the user Windows environment completed Debug and Release builds, focused tests, the full CTest suite, and the custom fused sample in both configurations. Phase 1 is complete.
