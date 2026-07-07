# D3D11View

`D3D11View` は、D3D11 の view object 作成を補助する advanced view helper です。

既存の `D3D11Helpers.hpp` には basic `CreateSrv` / `CreateUav` / `CreateRtv` / `CreateDsv` と Texture2D / Buffer 向けの簡易 helper があり、`D3D11View` はそれを壊さずに、array / cube / depth / raw buffer などの用途を補完します。

## Include

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11View.hpp>
```

または:

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
```

## Texture2D array views

```cpp
D3D11Texture2DArrayViewDesc desc = {};
desc.format = DXGI_FORMAT_UNKNOWN;
desc.mipSlice = 0;
desc.mostDetailedMip = 0;
desc.mipLevels = UINT(-1);
desc.firstArraySlice = 0;
desc.arraySize = UINT(-1);

auto srv = CreateTexture2DArraySrv(device, texture, desc);
auto uav = CreateTexture2DArrayUav(device, texture, desc);
auto rtv = CreateTexture2DArrayRtv(device, texture, desc);
auto dsv = CreateTexture2DArrayDsv(device, texture, desc);
```

`UINT(-1)` は、指定開始位置から残り全部を意味します。

## Cube views

```cpp
auto cubeSrv = CreateTextureCubeSrv(device, cubeTexture);

auto cubeArraySrv = CreateTextureCubeArraySrv(device, cubeArrayTexture);
```

Cube / cube-array texture は `D3D11_RESOURCE_MISC_TEXTURECUBE` を持つ `Texture2D` として作成します。

## Depth views

```cpp
auto dsv = CreateDepthTexture2DDsv(device, depthTexture);
auto srv = CreateDepthTexture2DSrv(device, depthTexture);
```

Depth texture を shader resource として読むには、resource format は typeless にするのが基本です。

例:

```cpp
DXGI_FORMAT_R24G8_TYPELESS      -> DSV: DXGI_FORMAT_D24_UNORM_S8_UINT
DXGI_FORMAT_R24G8_TYPELESS      -> depth SRV: DXGI_FORMAT_R24_UNORM_X8_TYPELESS
DXGI_FORMAT_R32_TYPELESS        -> DSV: DXGI_FORMAT_D32_FLOAT
DXGI_FORMAT_R32_TYPELESS        -> depth SRV: DXGI_FORMAT_R32_FLOAT
```

## Buffer views

```cpp
auto typedSrv = CreateTypedBufferSrv(device, buffer, DXGI_FORMAT_R32_FLOAT, 0, elementCount);
auto typedUav = CreateTypedBufferUav(device, buffer, DXGI_FORMAT_R32_FLOAT, 0, elementCount);

auto structuredSrv = CreateStructuredBufferSrv(device, buffer, 0, elementCount);
auto structuredUav = CreateStructuredBufferUav(device, buffer, 0, elementCount);

auto rawSrv = CreateRawBufferSrv(device, buffer, 0, dwordCount);
auto rawUav = CreateRawBufferUav(device, buffer, 0, dwordCount);
```

Raw buffer view の element は 32-bit 単位です。

## Scope

対応:

- Texture2D array SRV/UAV/RTV/DSV
- TextureCube SRV
- TextureCubeArray SRV
- depth DSV/SRV format mapping
- depth Texture2D / Texture2DArray DSV/SRV
- typed buffer SRV/UAV
- structured buffer SRV/UAV
- raw buffer SRV/UAV

非対応:

- Texture3D-specific helper
- automatic view cache
- descriptor heap abstraction
- shader reflection based automatic binding
- material/render-graph integration