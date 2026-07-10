# D3D11Helper v1.13.0 Release Checklist

## Implementation

- [x] Reusable NV12/P010 YUV HLSL primitives
- [x] Non-owning `D3D11ResourceView`
- [x] Texture2D validation
- [x] Processing `*View` entry points
- [x] Scoped staging map RAII
- [x] D3D11.4 Fence Point
- [x] Final compatibility audit

## Verification completed on user Windows environment

- [x] Debug build
- [x] Release build
- [x] focused foundation tests
- [x] full Debug CTest
- [x] full Release CTest
- [x] package smoke test
- [x] Release install
- [x] installed public-header checks
- [x] installed YUV shader-library check
- [x] Processing samples 07 / 08 / 25 in Debug
- [x] Processing samples 07 / 08 / 25 in Release

## Compatibility

- [x] no existing public type or method removed or renamed
- [x] no existing include path removed
- [x] no ambiguous same-name Processing overloads introduced
- [x] legacy staging `Map()` / `Unmap()` retained
- [x] legacy fence APIs retained
- [x] no explicit D3D12 resource-state model introduced

## Release metadata

- [x] CMake project version set to 1.13.0
- [x] README updated
- [x] v1.13.0 Release Notes added
- [x] final audit added
- [ ] dependent PRs merged in order
- [ ] release PR merged to `main`
- [ ] tag `v1.13.0` created from final `main` commit
- [ ] GitHub Release published
