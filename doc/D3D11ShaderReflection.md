# D3D11ShaderReflection

`D3D11ShaderReflection` provides lightweight reflection helpers for compiled shader bytecode.

## Include

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11ShaderReflection.hpp>
```

It is also included by:

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
```

## Basic usage

```cpp
using namespace D3D11CoreLib;

ShaderBytecode bytecode = CompileShaderFromSource_D3DCompile(
    hlslSource,
    "main",
    "vs_5_0");

ShaderReflectionInfo reflection = ReflectShaderBytecode(bytecode);
```

## Resource binding inspection

```cpp
const ShaderResourceBindingInfo* texture = FindResourceBinding(reflection, "gTexture");
if (texture) {
    UINT bindPoint = texture->bindPoint;
}
```

The binding info includes:

- resource name
- shader input type
- return type
- view dimension
- bind point
- bind count
- flags

`space` is included for D3D12 API-shape parity, but D3D11 always reports `0`.

## Constant buffer inspection

```cpp
const ShaderConstantBufferInfo* cb = FindConstantBuffer(reflection, "CameraConstants");
if (cb) {
    const ShaderConstantBufferVariableInfo* viewProj =
        FindConstantBufferVariable(*cb, "viewProj");
}
```

The constant buffer info includes:

- buffer name
- buffer type
- size in bytes
- variable list

Each variable includes:

- variable name
- start offset
- size in bytes
- flags

## Input layout generation

For vertex shaders, input signature parameters can be converted to a simple input-layout description.

```cpp
auto elements = MakeInputLayoutElementsFromReflection(reflection);
auto descs = MakeD3D11InputElementDescs(elements);
```

`MakeInputLayoutElementsFromReflection` skips system-value inputs such as `SV_VertexID` and maps common component masks to `DXGI_FORMAT_R32...` formats.

The returned `D3D11_INPUT_ELEMENT_DESC` objects reference semantic name strings owned by the `elements` vector. Keep `elements` alive while using the descriptors.

## Compute shader thread group size

```cpp
ShaderReflectionInfo reflection = ReflectShaderBytecode(computeBytecode);
UINT x = reflection.threadGroupSizeX;
UINT y = reflection.threadGroupSizeY;
UINT z = reflection.threadGroupSizeZ;
```

## Scope

D3D11 shader reflection uses `D3DReflect` and targets DXBC bytecode produced by `D3DCompile`. DXIL/DXC bytecode can still be compiled by `D3D11ShaderCompiler`, but it is not intended for D3D11 runtime pipeline creation.
