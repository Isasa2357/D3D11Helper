# D3D11 Generic GPU Foundation Phase 2: Non-owning Resource Views

Phase 2 adds a raw, non-owning resource view for caller-owned D3D11 resources and separately named Processing entry points that accept the view without changing existing `Dispatch*` signatures.

## Public resource view

Header:

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11ResourceView.hpp>
```

`D3D11ResourceView` contains one `ID3D11Resource*` and does not call `AddRef` or `Release` during construction, copy, assignment, or destruction.

```cpp
D3D11ResourceView view(externalTexture);
ID3D11Resource* raw = view.Get();
```

The external owner remains responsible for the resource lifetime.

## D3D11 ownership boundary

D3D11 differs from D3D12 in two important ways:

1. There is no explicit `D3D12_RESOURCE_STATES` equivalent to carry in the view API.
2. SRV and UAV objects created for a dispatch follow normal D3D11 COM ownership rules and may temporarily retain the underlying resource.

Therefore the contract is:

- `D3D11ResourceView` itself is strictly non-owning.
- Internal adapters never retain the borrowed base pointer after the call returns.
- Temporary SRV/UAV objects and borrowed-workspace protection may transiently change the COM count during the call.
- The COM count must return to its original value after the call completes, including exception unwinding.
- The caller must keep every external resource alive for the intended GPU use.
- For Deferred Context recording, the caller must keep resources alive through `FinishCommandList` and execution on the Immediate Context.

## Validation

Header:

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11ResourceValidation.hpp>
```

Added APIs:

- `D3D11Texture2DRequirement`
- `D3D11ValidationResult`
- `ValidateTexture2DView()`
- `ValidateTexture2DViewOrThrow()`
- owned-resource convenience wrappers `ValidateTexture2D()` and `ValidateTexture2DOrThrow()`

Validation covers:

- null and resource dimension,
- device identity,
- width, height, and multiple constraints,
- format, mip levels, array size,
- sample count and quality,
- usage,
- required and forbidden bind, CPU-access, and misc flags.

The result-returning API aggregates all applicable mismatches.

## Processing coverage

Every public Processing operation that dispatches work from caller-provided resources has a separately named view path.

### Basic and fused processing

- `DispatchConvertView`
- `DispatchResizeView`
- `DispatchRemapView`
- `DispatchCompositeView`
- `DispatchConvertResizeView`

### Effects and filters

- `DispatchBlurView`
- `DispatchRegionEffectView`
- `DispatchRegionBlurView`
- `DispatchColorAdjustView`
- `DispatchKernelFilterView`

### Mask and threshold processing

- `DispatchApplyMaskView`
- `DispatchBlendByMaskView`
- `DispatchCombineMasksView`
- `DispatchInvertMaskView`
- `DispatchThresholdView`
- `DispatchRangeThresholdView`
- `DispatchConfidenceHeatmapView`
- `DispatchClassColorMapView`
- `DispatchMaskOverlayView`

### Pyramid processing

- `DispatchDownsample2xView`
- `DispatchUpsample2xView`
- `D3D11PyramidBlurWorkspaceView`
- `DispatchPyramidBlurView`
- `D3D11PyramidRegionBlurWorkspaceView`
- `DispatchPyramidRegionBlurView`

Borrowed workspace views contain non-owning resource views. The adapter creates temporary owning references only for the duration of the multi-pass dispatch and releases them before returning.

### Advanced processing

- `DispatchAffineTransformView`
- `DispatchPerspectiveTransformView`
- `DispatchApplyLut3DView`
- `DispatchApplyUndistortMapView`

## Texture-view helpers

The following separately named helpers accept `D3D11ResourceView`:

- `CreateRgbaTextureViewSetView`
- `CreateYuv420SrvViewSetView`
- `CreateYuv420UavViewSetView`
- `CreateYuv420SrvUavViewSetView`

Returned SRV/UAV objects are owning COM objects. Destroying the returned view set releases those temporary references normally.

## Compatibility

- Existing `D3D11Resource` remains the owning `ComPtr` wrapper.
- Existing `Dispatch*` methods retain their original names and signatures.
- New entry points use distinct `*View` names, so existing address-of expressions remain unambiguous.
- No explicit resource-state or barrier model is introduced into D3D11.
- No header path is removed or renamed.

## Tests

`test_resource_view` covers:

- construction and copy without a net COM reference-count change,
- aggregate Texture2D validation,
- temporary Processing view creation without retained ownership,
- actual `DispatchConvertView` GPU execution and readback,
- compile-time addressability of every new Processing view entry point.

Phase 2 is complete after focused and full Debug/Release tests pass on the Windows validation environment.
