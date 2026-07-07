# D3D11State

`D3D11State` は、D3D11 state object 作成 helper と common descriptor presets を提供します。

## Include

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11State.hpp>
```

または:

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
```

## Creation helpers

```cpp
ComPtr<ID3D11SamplerState> CreateSamplerState(ID3D11Device* device, const D3D11_SAMPLER_DESC& desc);
ComPtr<ID3D11RasterizerState> CreateRasterizerState(ID3D11Device* device, const D3D11_RASTERIZER_DESC& desc);
ComPtr<ID3D11BlendState> CreateBlendState(ID3D11Device* device, const D3D11_BLEND_DESC& desc);
ComPtr<ID3D11DepthStencilState> CreateDepthStencilState(ID3D11Device* device, const D3D11_DEPTH_STENCIL_DESC& desc);
```

## StatePresets

Sampler presets:

```cpp
StatePresets::SamplerPointClamp();
StatePresets::SamplerPointWrap();
StatePresets::SamplerLinearClamp();
StatePresets::SamplerLinearWrap();
StatePresets::SamplerAnisotropicWrap(16);
StatePresets::SamplerComparisonLinearClamp();
```

Rasterizer presets:

```cpp
StatePresets::RasterizerCullBack();
StatePresets::RasterizerCullNone();
StatePresets::RasterizerWireframe();
StatePresets::RasterizerScissorCullBack();
```

Blend presets:

```cpp
StatePresets::BlendOpaque();
StatePresets::BlendAlpha();
StatePresets::BlendPremultipliedAlpha();
StatePresets::BlendAdditive();
```

Depth presets:

```cpp
StatePresets::DepthDisabled();
StatePresets::DepthDefault();
StatePresets::DepthReadOnly();
StatePresets::DepthReverseZ();
```

## Example

```cpp
auto sampler = CreateSamplerState(device, StatePresets::SamplerLinearClamp());
auto rasterizer = CreateRasterizerState(device, StatePresets::RasterizerCullBack());
auto blend = CreateBlendState(device, StatePresets::BlendAlpha());
auto depth = CreateDepthStencilState(device, StatePresets::DepthDefault());
```

## Scope

`D3D11State` は state object と descriptor preset の薄い helper です。Pipeline object、material system、state cache、render graph は扱いません。