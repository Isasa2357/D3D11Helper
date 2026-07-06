# Migration Guide - v1.2.0

v1.2.0 is backward compatible with v1.1.0. Existing code can continue to use the same APIs.

This guide is for code that wants to adopt the new transfer APIs.

## Before v1.2.0

Readback code often used `D3D11StagingBuffer` directly:

```cpp
D3D11StagingBuffer staging;
staging.InitializeAsTexture2D(core->GetDevice(), width, height, format);

core->GetImmediateContext()->CopyResource(staging.Get(), texture.Get());
core->Flush();

const auto* mapped = static_cast<const uint8_t*>(staging.Map(core->GetImmediateContext()));
UINT rowPitch = staging.GetMappedRowPitch();

// Manual row-pitch handling here.

staging.Unmap(core->GetImmediateContext());
```

## v1.2.0

Use `ReadbackTexture2DToCpuImage`:

```cpp
auto image = ReadbackTexture2DToCpuImage(*core, texture);
```

The result is a CPU image with packed or explicitly described row pitch.

## Upload

Before v1.2.0, upload was usually done from raw memory:

```cpp
auto tex = CreateTexture2DFromMemory(*core, data, width, height, format, rowPitch);
```

v1.2.0 adds a structured CPU image path:

```cpp
auto image = CreateCpuImage(width, height, DXGI_FORMAT_R8G8B8A8_UNORM);
// fill image.pixels

auto tex = CreateTexture2DFromCpuImage(*core, image);
```

## Update

```cpp
UpdateTexture2DFromCpuImage(*core, tex, image);
```

This is useful when the caller already uses `D3D11CpuImage` as a common interchange object.

## File I/O

Do not add file I/O directly to D3D11Helper.

Use `D3D11CpuImage` as the handoff type for upper libraries:

```text
D3D11Helper
  -> D3D11CpuImage
  -> D3DTextureIO / D3DVideoIO / application-specific encoder
```
