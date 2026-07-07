# Migration Guide - D3D11Helper v1.7.0

`v1.7.0` is an additive release. Existing code using v1.6.0 should continue to build without source changes.

## New include options

Use the `D3D11Gpu` umbrella header:

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
```

Or include the new headers directly:

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11View.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11State.hpp>
```

## Migrating advanced view creation

Existing full-desc APIs remain valid:

```cpp
CreateSrv(core, resource, desc);
CreateUav(core, resource, desc);
CreateRtv(core, resource, desc);
CreateDsv(core, resource, desc);
```

For common advanced views, new code can use `D3D11View` helpers:

```cpp
auto srv = CreateTexture2DArraySrv(device, textureArray);
auto cubeSrv = CreateTextureCubeSrv(device, cubeTexture);
auto depthSrv = CreateDepthTexture2DSrv(device, depthTexture);
auto rawUav = CreateRawBufferUav(device, buffer, 0, dwordCount);
```

## Migrating state descriptor creation

Existing `PipelineDefaults` functions remain available for graphics pipeline setup.

For reusable standalone state objects, use `D3D11State`:

```cpp
auto sampler = CreateSamplerState(device, StatePresets::SamplerLinearClamp());
auto blend = CreateBlendState(device, StatePresets::BlendAlpha());
```

## Notes

- Depth textures intended for both DSV and SRV should normally use typeless resource formats.
- Raw buffer view element counts are expressed in 32-bit units.
- The new helpers perform validation and throw exceptions for invalid ranges, missing bind flags, unsupported formats, and null pointers.