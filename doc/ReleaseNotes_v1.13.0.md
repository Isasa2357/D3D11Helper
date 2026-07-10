# D3D11Helper v1.13.0 Release Notes

## Overview

v1.13.0 adds the D3D11-side generic GPU foundation corresponding to the functional categories introduced in D3D12Helper v1.13.0, while preserving Direct3D 11's natural programming model.

This release does not mechanically copy D3D12 resource states, barriers, command allocators, or typed command lists into D3D11. Instead, it adds the D3D11 equivalents that are useful in real applications.

## New features

### Reusable YUV HLSL primitives

`shaders/D3D11Processing/YuvPrimitives.hlsli` provides reusable functions for application-owned fused shaders.

Supported combinations:

- NV12 and P010
- BT.601, BT.709, and BT.2020
- full and limited range
- point and linear YUV420 sampling
- point and linear logical RGBA sampling
- Y-plane and UV-plane stores

P010 conversion explicitly handles the 10-bit video code stored in bits 15:6 of each 16-bit word.

Existing Processing shaders and the custom fused sample use the new library without changing shader filenames, constant-buffer layout, or resource slots.

### Non-owning resource views

`D3D11ResourceView` references caller-owned `ID3D11Resource` objects without calling `AddRef` or `Release` during view construction, copying, assignment, or destruction.

All Processing processors now provide separately named `*View` entry points. Existing `Dispatch*` methods remain unchanged, so prior address-of expressions and source code remain unambiguous.

D3D11 SRV/UAV objects continue to follow normal COM ownership rules and may temporarily retain the resource during a dispatch. The net reference count returns to its original value after the temporary views are released.

### Texture2D validation

New APIs:

- `D3D11Texture2DRequirement`
- `D3D11ValidationResult`
- `ValidateTexture2DView()`
- `ValidateTexture2DViewOrThrow()`
- owned-resource convenience wrappers

Validation can check:

- null and resource dimension
- D3D11 device identity
- exact size and size multiples
- format
- mip count and array size
- sample count and quality
- usage
- required and forbidden bind flags
- CPU access flags
- miscellaneous flags

The result form aggregates multiple errors into one report.

### Scoped staging map

`D3D11MappedSubresource` and `D3D11StagingBuffer::MapScoped()` provide move-only RAII mapping.

Features:

- automatic `Unmap()` at scope exit
- explicit `Reset()`
- move construction and move assignment
- typed data access
- row-pitch and depth-pitch access
- mapped-state query

The existing manual `Map()` / `Unmap()` API and repeated-map behavior remain unchanged.

### Fence points

`D3D11FencePoint` stores an `ID3D11Fence` and fence value as an immutable synchronization token.

New APIs:

- `D3D11Fence::SignalPoint()`
- `D3D11Fence::GpuWaitPoint()`
- `D3D11FencePoint::IsComplete()`
- `D3D11FencePoint::CpuWait()`

A point retains the underlying fence independently of the wrapper that created it.

Fence points belong to `D3D11Interop`. They are intended for D3D11.4 / D3D12 interoperability and synchronization between independently progressing contexts. Waiting on a value that only a later command on the same Immediate Context can signal causes a self-deadlock and must be avoided.

## Compatibility

v1.13.0 preserves the existing v1.x public surface:

- no public type, function, method, or include path was removed or renamed
- `D3D11Resource` remains an owning `ComPtr` wrapper
- existing Processing `Dispatch*` signatures are unchanged
- existing `D3D11StagingBuffer::Map()` / `Unmap()` signatures and behavior are unchanged
- existing `D3D11Fence::Signal()`, `GpuWait()`, and `CpuWait()` signatures are unchanged
- no same-name overload was added where it would make an existing address-of expression ambiguous
- no D3D12-style explicit resource-state requirement was introduced

## Verification

The integrated foundation branch passed on the user Windows environment:

- Debug and Release builds
- focused foundation tests
- full Debug and Release CTest runs
- package smoke test
- Release install
- installed public-header and shader checks
- Processing samples 07, 08, and 25 in Debug and Release

D3D11.4 Fence runtime cases may be skipped on unsupported adapters or WARP while the test suite remains successful.

## Intentional exclusions

The following remain outside D3D11Helper:

- image-file loading and saving
- video codec, muxing, and container APIs
- Media Foundation, NVENC, FFmpeg, OpenCV, or camera-session integration
- D3D12-style resource-state tracking and barrier batching
- command allocator or typed command-list abstractions

These belong in higher-level libraries or are not natural Direct3D 11 abstractions.
