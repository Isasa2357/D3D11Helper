# D3D11 Generic GPU Foundation Final Audit

## Scope

This audit covers the D3D11-side generic GPU foundation work implemented after the D3D12Helper v1.13.0 foundation release.

The goal is functional-category parity where it is natural for Direct3D 11, not mechanical API parity with Direct3D 12.

## Completed phases

### Phase 1: Reusable YUV HLSL primitives

- NV12 and P010 video-code aware conversion
- BT.601, BT.709, and BT.2020 matrices
- Full and limited range handling
- Point and linear YUV420 sampling
- Logical RGBA point and linear sampling
- Y and UV plane storage helpers
- CPU golden tests, GPU probe tests, and real planar storage readback tests
- D3DCompile / Shader Model 5 compatibility

### Phase 2: Non-owning resource views and validation

- `D3D11ResourceView`
- `D3D11Texture2DRequirement`
- `D3D11ValidationResult`
- aggregate and throwing Texture2D validation
- separately named Processing `*View` entry points
- borrowed pyramid workspace views
- COM reference-count and exception-unwinding coverage

### Phase 3: Scoped staging map

- move-only `D3D11MappedSubresource`
- `D3D11StagingBuffer::MapScoped()`
- automatic `Unmap()` on destruction or `Reset()`
- data, row-pitch, and depth-pitch accessors
- legacy manual `Map()` / `Unmap()` behavior retained

### Phase 4: Fence points

- immutable `D3D11FencePoint`
- `D3D11Fence::SignalPoint()`
- `D3D11Fence::GpuWaitPoint()`
- point-side completion check and CPU wait
- fence lifetime retained by the point
- D3D11 single-timeline deadlock constraints documented

## Intentional D3D11 differences

The following D3D12 concepts are intentionally not ported:

- resource-state descriptors
- resource-barrier batching
- command allocator abstraction
- typed command-list creation
- queue objects for ordinary D3D11 work submission

Direct3D 11 performs implicit resource-state management and uses Immediate / Deferred Contexts instead of D3D12 command allocators and typed command lists.

`D3D11FencePoint` is kept in the Interop module because `ID3D11Fence` is primarily relevant to D3D11.4 and D3D11/D3D12 interoperability rather than ordinary D3D11 frame submission.

## Compatibility audit

The following compatibility requirements are satisfied:

- no existing public type, function, method, or include path was removed or renamed
- existing `D3D11Resource` remains an owning `ComPtr` wrapper
- existing Processing `Dispatch*` method names and signatures remain unchanged
- new Processing entry points use distinct `*View` names rather than same-name overloads
- existing `D3D11StagingBuffer::Map()` / `Unmap()` signatures and repeated-map behavior remain unchanged
- existing `D3D11Fence::Signal()`, `GpuWait()`, and `CpuWait()` signatures remain unchanged
- no D3D12-style explicit resource-state model was introduced
- no codec, container, encoder, decoder, or media-session API was added

## Verification

The integrated `feature/d3d11-fence-point` branch passed on the user Windows environment:

- Debug build
- Release build
- focused foundation CTest set
- full Debug CTest
- full Release CTest
- package smoke test
- Release install
- installed public-header and shader checks
- Processing samples 07, 08, and 25 in Debug and Release

Fence runtime tests may skip unsupported D3D11.4 or WARP-only cases while the CTest itself remains successful.

## Release readiness

Phases 1 through 4 are implementation-complete and Windows-verified. The remaining work before merge is release metadata review and ordered merge of the dependent pull requests.
