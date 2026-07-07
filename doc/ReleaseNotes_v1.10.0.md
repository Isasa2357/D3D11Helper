# D3D11Helper v1.10.0 Release Notes

## Summary

v1.10.0 adds lightweight shader reflection helpers.

## Added

- `D3D11ShaderReflection.hpp`.
- `ReflectShaderBytecode` for compiled DXBC bytecode.
- Resource binding inspection.
- Constant buffer and variable inspection.
- Input and output signature parameter inspection.
- Input layout element generation from vertex shader signatures.
- Compute shader thread group size inspection.
- Shader reflection tests.

## Notes

D3D11 shader reflection uses `D3DReflect` and targets DXBC bytecode produced by `D3DCompile`.
