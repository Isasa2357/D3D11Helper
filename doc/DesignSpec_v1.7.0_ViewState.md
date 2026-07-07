# Design Spec: D3D11Helper v1.7.0 View / State Expansion

## 1. Goal

v1.7.0 extends the `D3D11Gpu` layer with more complete view and state helpers.

The existing library already has basic view helpers in `D3D11Helpers.hpp`, including full-desc SRV/UAV/RTV/DSV creation and simple Texture2D / Buffer helpers. v1.7.0 should add focused helpers for cases that are still cumbersome or error-prone in raw D3D11:

- Texture2D array views
- cube texture and cube array SRVs
- depth texture SRV/DSV format mapping helpers
- typed / structured / raw buffer SRV/UAV helpers
- sampler / rasterizer / blend / depth-stencil state presets
- state object creation helpers

The API should remain a thin Direct3D 11 helper layer. It should not become a render graph, material system, or high-level renderer.

## 2. Non-goals

Do not implement:

- file I/O or image loading
- WIC / PNG / JPEG / DDS helpers
- Media Foundation / FFmpeg / OpenCV integration
- CUDA / NVENC / vendor-specific paths
- shader-based processing
- render graph or frame graph
- automatic hazard tracking
- automatic resource lifetime management beyond returning `ComPtr`
- GUI / windowed samples
- release notes, migration guide, README release section, or version bump in this Codex pass

## 3. Module placement

Add new files:

```text
include/D3D11Helper/D3D11Gpu/D3D11View.hpp
include/D3D11Helper/D3D11Gpu/D3D11State.hpp

src/D3D11View.cpp
src/D3D11State.cpp
```

Update:

```text
CMakeLists.txt
include/D3D11Helper/D3D11Gpu/D3D11Gpu.hpp
Test/CMakeLists.txt
Test/test_module_headers.cpp
sample/CMakeLists.txt
```

Add:

```text
Test/test_view_state.cpp
sample/23_ViewState/main.cpp
```

## 4. Namespace

Use the existing namespace:

```cpp
namespace D3D11CoreLib {
// public APIs
}
```

For state presets, use a nested namespace to avoid clashing with existing helper names:

```cpp
namespace D3D11CoreLib {
namespace StatePresets {
// preset desc factories
}
}
```

Do not rename or remove the existing `PipelineDefaults` namespace in `D3D11GraphicsPipeline.hpp`.

## 5. Relationship to existing APIs

Existing helpers in `D3D11Helpers.hpp` must continue to compile and behave as before.

Do not remove or rename:

- `CreateSrv`
- `CreateUav`
- `CreateRtv`
- `CreateDsv`
- `CreateTexture2DSrv`
- `CreateTexture2DUav`
- `CreateTexture2DRtv`
- `CreateTexture2DDsv`
- `CreateBufferSrv`
- `CreateBufferUav`
- `MakeLinearClampSamplerDesc`
- `MakePointClampSamplerDesc`
- `CreateSampler`

The v1.7.0 helpers may internally duplicate small descriptor-building logic, but should not break compatibility.

Optional but acceptable: existing pipeline defaults may call into the new state preset functions internally, as long as public behavior remains compatible. This is not required.

## 6. Error handling

Public functions should validate arguments.

Use:

- `std::invalid_argument` for null pointers and logically invalid arguments
- `std::out_of_range` for mip / array / element ranges outside resource bounds
- `D3D11CORE_THROW_IF_FAILED` for HRESULT-returning D3D calls

Avoid silent no-op behavior.

## 7. D3D11View API

### 7.1 Header

File:

```text
include/D3D11Helper/D3D11Gpu/D3D11View.hpp
```

Include:

```cpp
#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>
```

### 7.2 Descriptor structs

Use lightweight descriptor structs. Suggested API:

```cpp
struct D3D11Texture2DArrayViewDesc {
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT mipSlice = 0;          // for RTV/UAV/DSV
    UINT mostDetailedMip = 0;   // for SRV
    UINT mipLevels = UINT(-1);  // for SRV
    UINT firstArraySlice = 0;
    UINT arraySize = UINT(-1);
};

struct D3D11TextureCubeViewDesc {
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT mostDetailedMip = 0;
    UINT mipLevels = UINT(-1);
};

struct D3D11TextureCubeArrayViewDesc {
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT mostDetailedMip = 0;
    UINT mipLevels = UINT(-1);
    UINT firstCube = 0;
    UINT cubeCount = UINT(-1);
};

struct D3D11BufferViewDesc {
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT firstElement = 0;
    UINT numElements = 0;
    UINT flags = 0;
};
```

If Codex wants to rename these slightly for clarity, that is acceptable, but the API should remain simple and stable-looking.

### 7.3 Texture2D array view helpers

Implement:

```cpp
ComPtr<ID3D11ShaderResourceView> CreateTexture2DArraySrv(
    ID3D11Device* device,
    ID3D11Texture2D* texture,
    const D3D11Texture2DArrayViewDesc& desc = {});

ComPtr<ID3D11UnorderedAccessView> CreateTexture2DArrayUav(
    ID3D11Device* device,
    ID3D11Texture2D* texture,
    const D3D11Texture2DArrayViewDesc& desc = {});

ComPtr<ID3D11RenderTargetView> CreateTexture2DArrayRtv(
    ID3D11Device* device,
    ID3D11Texture2D* texture,
    const D3D11Texture2DArrayViewDesc& desc = {});

ComPtr<ID3D11DepthStencilView> CreateTexture2DArrayDsv(
    ID3D11Device* device,
    ID3D11Texture2D* texture,
    const D3D11Texture2DArrayViewDesc& desc = {});
```

Validation:

- `device` and `texture` are non-null
- `textureDesc.ArraySize > 0`
- selected array range is valid
- selected mip range is valid
- for UAV/RTV/DSV, `mipSlice < MipLevels`
- for SRV, `mostDetailedMip < MipLevels`
- `mipLevels == UINT(-1)` means all remaining mips
- `arraySize == UINT(-1)` means all remaining array slices
- default `format == DXGI_FORMAT_UNKNOWN` uses `textureDesc.Format`

View dimensions:

- SRV: `D3D11_SRV_DIMENSION_TEXTURE2DARRAY`
- UAV: `D3D11_UAV_DIMENSION_TEXTURE2DARRAY`
- RTV: `D3D11_RTV_DIMENSION_TEXTURE2DARRAY`
- DSV: `D3D11_DSV_DIMENSION_TEXTURE2DARRAY`

For multisample textures, use the corresponding `TEXTURE2DMSARRAY` view dimensions for SRV/RTV/DSV when applicable. UAV is not valid for multisample textures and should throw.

### 7.4 Cube texture helpers

Implement:

```cpp
ComPtr<ID3D11ShaderResourceView> CreateTextureCubeSrv(
    ID3D11Device* device,
    ID3D11Texture2D* texture,
    const D3D11TextureCubeViewDesc& desc = {});

ComPtr<ID3D11ShaderResourceView> CreateTextureCubeArraySrv(
    ID3D11Device* device,
    ID3D11Texture2D* texture,
    const D3D11TextureCubeArrayViewDesc& desc = {});
```

Validation:

- `device` and `texture` are non-null
- `textureDesc.MiscFlags` contains `D3D11_RESOURCE_MISC_TEXTURECUBE`
- `textureDesc.ArraySize` is a multiple of 6
- cube range is valid
- mip range is valid
- default format uses texture format

View dimensions:

- cube SRV: `D3D11_SRV_DIMENSION_TEXTURECUBE`
- cube array SRV: `D3D11_SRV_DIMENSION_TEXTURECUBEARRAY`

If cube array view creation is not supported by the active device / feature level, the D3D call may fail. Use `D3D11CORE_THROW_IF_FAILED`.

### 7.5 Depth format helpers

Depth textures usually need typeless resource formats and typed DSV/SRV formats. Implement mapping helpers:

```cpp
bool IsDepthStencilViewFormat(DXGI_FORMAT format) noexcept;
bool IsTypelessDepthTextureFormat(DXGI_FORMAT format) noexcept;

DXGI_FORMAT GetTypelessDepthTextureFormat(DXGI_FORMAT format) noexcept;
DXGI_FORMAT GetDepthStencilViewFormat(DXGI_FORMAT format) noexcept;
DXGI_FORMAT GetDepthShaderResourceViewFormat(DXGI_FORMAT format) noexcept;
DXGI_FORMAT GetStencilShaderResourceViewFormat(DXGI_FORMAT format) noexcept;
```

Required mappings:

```text
R32_TYPELESS / D32_FLOAT / R32_FLOAT
  typeless texture: R32_TYPELESS
  DSV:              D32_FLOAT
  depth SRV:        R32_FLOAT
  stencil SRV:      UNKNOWN

R24G8_TYPELESS / D24_UNORM_S8_UINT / R24_UNORM_X8_TYPELESS / X24_TYPELESS_G8_UINT
  typeless texture: R24G8_TYPELESS
  DSV:              D24_UNORM_S8_UINT
  depth SRV:        R24_UNORM_X8_TYPELESS
  stencil SRV:      X24_TYPELESS_G8_UINT

R16_TYPELESS / D16_UNORM / R16_UNORM
  typeless texture: R16_TYPELESS
  DSV:              D16_UNORM
  depth SRV:        R16_UNORM
  stencil SRV:      UNKNOWN

R32G8X24_TYPELESS / D32_FLOAT_S8X24_UINT / R32_FLOAT_X8X24_TYPELESS / X32_TYPELESS_G8X24_UINT
  typeless texture: R32G8X24_TYPELESS
  DSV:              D32_FLOAT_S8X24_UINT
  depth SRV:        R32_FLOAT_X8X24_TYPELESS
  stencil SRV:      X32_TYPELESS_G8X24_UINT
```

For unsupported formats, return `DXGI_FORMAT_UNKNOWN` rather than throwing from the pure mapping functions.

### 7.6 Depth view helpers

Implement:

```cpp
ComPtr<ID3D11DepthStencilView> CreateDepthTexture2DDsv(
    ID3D11Device* device,
    ID3D11Texture2D* texture,
    UINT mipSlice = 0,
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN,
    UINT flags = 0);

ComPtr<ID3D11ShaderResourceView> CreateDepthTexture2DSrv(
    ID3D11Device* device,
    ID3D11Texture2D* texture,
    UINT mostDetailedMip = 0,
    UINT mipLevels = UINT(-1),
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);
```

Format defaulting:

- DSV default should use `GetDepthStencilViewFormat(textureDesc.Format)`
- depth SRV default should use `GetDepthShaderResourceViewFormat(textureDesc.Format)`
- if the mapped format is `DXGI_FORMAT_UNKNOWN`, throw `std::invalid_argument`

### 7.7 Buffer view helpers

Implement explicit buffer view helpers:

```cpp
ComPtr<ID3D11ShaderResourceView> CreateTypedBufferSrv(
    ID3D11Device* device,
    ID3D11Buffer* buffer,
    DXGI_FORMAT format,
    UINT firstElement,
    UINT numElements);

ComPtr<ID3D11UnorderedAccessView> CreateTypedBufferUav(
    ID3D11Device* device,
    ID3D11Buffer* buffer,
    DXGI_FORMAT format,
    UINT firstElement,
    UINT numElements,
    UINT flags = 0);

ComPtr<ID3D11ShaderResourceView> CreateStructuredBufferSrv(
    ID3D11Device* device,
    ID3D11Buffer* buffer,
    UINT firstElement,
    UINT numElements);

ComPtr<ID3D11UnorderedAccessView> CreateStructuredBufferUav(
    ID3D11Device* device,
    ID3D11Buffer* buffer,
    UINT firstElement,
    UINT numElements,
    UINT flags = 0);

ComPtr<ID3D11ShaderResourceView> CreateRawBufferSrv(
    ID3D11Device* device,
    ID3D11Buffer* buffer,
    UINT firstElement,
    UINT numElements);

ComPtr<ID3D11UnorderedAccessView> CreateRawBufferUav(
    ID3D11Device* device,
    ID3D11Buffer* buffer,
    UINT firstElement,
    UINT numElements,
    UINT flags = D3D11_BUFFER_UAV_FLAG_RAW);
```

Validation:

- `device` and `buffer` are non-null
- `numElements > 0`
- `firstElement + numElements` fits the buffer based on stride/format rules when possible
- typed views require `format != DXGI_FORMAT_UNKNOWN`
- structured views require `StructureByteStride > 0` and `D3D11_RESOURCE_MISC_BUFFER_STRUCTURED`
- raw views require `D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS`
- raw views use `DXGI_FORMAT_R32_TYPELESS`
- raw element range is in units of 4 bytes

For typed view range validation, use `FormatUtil::BytesPerPixel(format)` where appropriate. If bytes-per-element is unknown, still validate `numElements > 0` and let D3D11 validate exact compatibility.

## 8. D3D11State API

### 8.1 Header

File:

```text
include/D3D11Helper/D3D11Gpu/D3D11State.hpp
```

Include:

```cpp
#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>
```

### 8.2 State creation functions

Implement flat creation functions:

```cpp
ComPtr<ID3D11SamplerState> CreateSamplerState(
    ID3D11Device* device,
    const D3D11_SAMPLER_DESC& desc);

ComPtr<ID3D11RasterizerState> CreateRasterizerState(
    ID3D11Device* device,
    const D3D11_RASTERIZER_DESC& desc);

ComPtr<ID3D11BlendState> CreateBlendState(
    ID3D11Device* device,
    const D3D11_BLEND_DESC& desc);

ComPtr<ID3D11DepthStencilState> CreateDepthStencilState(
    ID3D11Device* device,
    const D3D11_DEPTH_STENCIL_DESC& desc);
```

These should validate `device != nullptr` and use `D3D11CORE_THROW_IF_FAILED`.

### 8.3 State preset namespace

Implement descriptor preset functions in:

```cpp
namespace D3D11CoreLib::StatePresets {
// desc factory functions
}
```

Suggested sampler presets:

```cpp
D3D11_SAMPLER_DESC SamplerPointClamp();
D3D11_SAMPLER_DESC SamplerPointWrap();
D3D11_SAMPLER_DESC SamplerLinearClamp();
D3D11_SAMPLER_DESC SamplerLinearWrap();
D3D11_SAMPLER_DESC SamplerAnisotropicWrap(UINT maxAnisotropy = 16);
D3D11_SAMPLER_DESC SamplerComparisonLinearClamp(
    D3D11_COMPARISON_FUNC comparison = D3D11_COMPARISON_LESS_EQUAL);
```

Suggested rasterizer presets:

```cpp
D3D11_RASTERIZER_DESC RasterizerCullBack(
    bool frontCounterClockwise = false);
D3D11_RASTERIZER_DESC RasterizerCullNone();
D3D11_RASTERIZER_DESC RasterizerWireframe();
D3D11_RASTERIZER_DESC RasterizerScissorCullBack(
    bool frontCounterClockwise = false);
```

Suggested blend presets:

```cpp
D3D11_BLEND_DESC BlendOpaque();
D3D11_BLEND_DESC BlendAlpha();
D3D11_BLEND_DESC BlendPremultipliedAlpha();
D3D11_BLEND_DESC BlendAdditive();
```

Suggested depth-stencil presets:

```cpp
D3D11_DEPTH_STENCIL_DESC DepthDisabled();
D3D11_DEPTH_STENCIL_DESC DepthDefault(
    bool depthWrite = true,
    D3D11_COMPARISON_FUNC depthFunc = D3D11_COMPARISON_LESS);
D3D11_DEPTH_STENCIL_DESC DepthReadOnly(
    D3D11_COMPARISON_FUNC depthFunc = D3D11_COMPARISON_LESS_EQUAL);
D3D11_DEPTH_STENCIL_DESC DepthReverseZ(
    bool depthWrite = true);
```

These names are intentionally distinct from existing global `MakeLinearClampSamplerDesc()` / `MakePointClampSamplerDesc()` helpers.

### 8.4 Descriptor defaults

Use robust Direct3D 11 defaults:

- sampler `MinLOD = 0`, `MaxLOD = D3D11_FLOAT32_MAX`
- rasterizer `DepthClipEnable = TRUE`
- blend `RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL`
- depth default should enable depth test and optionally depth writes
- depth disabled should set `DepthEnable = FALSE`, `DepthWriteMask = ZERO`

For all blend presets, initialize all eight render target descriptors to safe defaults, then configure render target 0.

## 9. CMake / umbrella registration

Update root `CMakeLists.txt`:

```text
src/D3D11View.cpp
src/D3D11State.cpp
include/D3D11Helper/D3D11Gpu/D3D11View.hpp
include/D3D11Helper/D3D11Gpu/D3D11State.hpp
```

Update:

```text
include/D3D11Helper/D3D11Gpu/D3D11Gpu.hpp
```

Add:

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11View.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11State.hpp>
```

## 10. Tests

Add:

```text
Test/test_view_state.cpp
```

Register it in `Test/CMakeLists.txt`:

```cmake
add_d3d11_test(test_view_state test_view_state.cpp 90)
```

Test coverage should include:

### 10.1 Header smoke

Update `Test/test_module_headers.cpp` to instantiate or reference:

```cpp
D3D11Texture2DArrayViewDesc
D3D11TextureCubeViewDesc
D3D11TextureCubeArrayViewDesc
D3D11BufferViewDesc
```

And call at least one `StatePresets` function in a compile-only style.

### 10.2 Texture2D array views

Create a Texture2D array with `ArraySize >= 2` and compatible bind flags.

Test:

- array SRV creation
- array RTV creation when bind flags include `D3D11_BIND_RENDER_TARGET`
- array UAV creation when bind flags include `D3D11_BIND_UNORDERED_ACCESS`

### 10.3 Cube SRV

Create a Texture2D with:

- `ArraySize = 6`
- `MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE`
- `D3D11_BIND_SHADER_RESOURCE`

Create cube SRV.

Cube array SRV may be optional or guarded if needed.

### 10.4 Depth views

Create a typeless depth texture, for example:

```text
DXGI_FORMAT_R24G8_TYPELESS
BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE
```

Create:

- DSV with default depth mapping
- depth SRV with default depth mapping
- optionally stencil SRV if supported

### 10.5 Raw / structured / typed buffer views

Create:

- structured buffer with `D3D11_RESOURCE_MISC_BUFFER_STRUCTURED`, nonzero `StructureByteStride`, SRV/UAV bind flags
- raw buffer with `D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS`, `ByteWidth` multiple of 4, SRV/UAV bind flags
- typed buffer view where format and byte size are compatible

Create SRV/UAV for each appropriate case.

### 10.6 State presets

Create actual D3D11 state objects from presets:

- sampler states
- rasterizer states
- blend states
- depth-stencil states

Also add invalid argument checks for null device / invalid ranges.

## 11. Sample

Add:

```text
sample/23_ViewState/main.cpp
```

Register it in `sample/CMakeLists.txt`:

```cmake
d3d11helper_add_sample(D3D11Sample_23_ViewState 23_ViewState)
```

The sample should be console-only and demonstrate:

- creating a Texture2D array SRV/RTV
- creating a cube SRV
- creating a raw or structured buffer SRV/UAV
- creating several state objects from `StatePresets`

Do not create a window.
Do not read or write files.
Do not require shader compilation.
Print concise status messages and `RESULT: OK` on success.

## 12. Final audit required in Codex summary

Codex must explicitly report:

```text
D3D11View and D3D11State are registered in CMakeLists.txt source and public header lists.
D3D11View and D3D11State are included from include/D3D11Helper/D3D11Gpu/D3D11Gpu.hpp.
Test/test_view_state.cpp is registered in Test/CMakeLists.txt.
sample/23_ViewState/main.cpp is registered in sample/CMakeLists.txt.
```

Also report:

```text
CMakeLists.txt project version was not changed.
README.md / CHANGELOG.md / release notes / migration guide were not changed in this pass.
```
