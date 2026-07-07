#pragma once
//
// D3D11SharedTexture.hpp - shared Texture2D creation/open helpers.
//
// These helpers stay DirectX/DXGI-only. They do not depend on D3D12Helper,
// D3D11On12, CUDA, media APIs, or external processes.
//
#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>
#include <D3D11Helper/D3D11Interop/D3D11SharedHandle.hpp>

namespace D3D11CoreLib {

enum class D3D11SharedTextureSyncMode {
    SharedFence,
    KeyedMutex
};

struct D3D11SharedTexture2DDesc {
    UINT width = 0;
    UINT height = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

    UINT bindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
    UINT cpuAccessFlags = 0;

    UINT mipLevels = 1;
    UINT arraySize = 1;
    UINT sampleCount = 1;
    UINT sampleQuality = 0;

    // Extra non-sharing misc flags. Do not put SHARED_NTHANDLE / SHARED /
    // SHARED_KEYEDMUTEX here; use syncMode instead.
    UINT miscFlags = 0;
    D3D11SharedTextureSyncMode syncMode = D3D11SharedTextureSyncMode::SharedFence;
};

struct D3D11SharedTexture2D {
    ComPtr<ID3D11Texture2D> texture;
    D3D11SharedHandle handle;

    ID3D11Texture2D* Get() const noexcept { return texture.Get(); }
    HANDLE GetHandle() const noexcept { return handle.Get(); }
    explicit operator bool() const noexcept { return texture != nullptr && handle.IsValid(); }
};

UINT MakeSharedTextureMiscFlags(
    D3D11SharedTextureSyncMode syncMode,
    UINT extraMiscFlags = 0);

void ValidateSharedTexture2DDesc(
    const D3D11SharedTexture2DDesc& desc,
    const char* functionName);

ComPtr<ID3D11Texture2D> CreateSharedTexture2D(
    ID3D11Device* device,
    const D3D11SharedTexture2DDesc& desc);

D3D11SharedTexture2D CreateSharedTexture2DWithHandle(
    ID3D11Device* device,
    const D3D11SharedTexture2DDesc& desc,
    LPCWSTR name = nullptr);

ComPtr<ID3D11Texture2D> OpenSharedTexture2D(
    ID3D11Device* device,
    HANDLE handle);

ComPtr<ID3D11Texture2D> OpenSharedTexture2D(
    ID3D11Device* device,
    const D3D11SharedHandle& handle);

} // namespace D3D11CoreLib
