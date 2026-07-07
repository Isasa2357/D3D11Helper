//
// D3D11SharedTexture.cpp
//
#include <D3D11Helper/D3D11Interop/D3D11SharedTexture.hpp>
#include <D3D11Helper/D3D11Interop/D3D11SharedResource.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>

#include <stdexcept>
#include <string>

namespace D3D11CoreLib {

namespace {

void Require(bool condition, const char* functionName, const char* message) {
    if (!condition) {
        throw std::invalid_argument(std::string(functionName) + ": " + message);
    }
}

constexpr UINT kSharingFlags =
    D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
    D3D11_RESOURCE_MISC_SHARED |
    D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

} // namespace

UINT MakeSharedTextureMiscFlags(
    D3D11SharedTextureSyncMode syncMode,
    UINT extraMiscFlags) {

    const UINT extra = extraMiscFlags & ~kSharingFlags;

    switch (syncMode) {
    case D3D11SharedTextureSyncMode::SharedFence:
        return extra | D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED;
    case D3D11SharedTextureSyncMode::KeyedMutex:
        return extra | D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
    default:
        throw std::invalid_argument("MakeSharedTextureMiscFlags: unknown sync mode");
    }
}

void ValidateSharedTexture2DDesc(
    const D3D11SharedTexture2DDesc& desc,
    const char* functionName) {

    Require(desc.width > 0, functionName, "width must be greater than zero");
    Require(desc.height > 0, functionName, "height must be greater than zero");
    Require(desc.format != DXGI_FORMAT_UNKNOWN, functionName, "format must not be DXGI_FORMAT_UNKNOWN");
    Require(desc.mipLevels > 0, functionName, "mipLevels must be greater than zero");
    Require(desc.arraySize > 0, functionName, "arraySize must be greater than zero");
    Require(desc.sampleCount > 0, functionName, "sampleCount must be greater than zero");
    Require(desc.usage == D3D11_USAGE_DEFAULT, functionName, "shared textures must use D3D11_USAGE_DEFAULT");
    Require(desc.cpuAccessFlags == 0, functionName, "shared textures must not use CPU access flags");
    Require((desc.miscFlags & kSharingFlags) == 0,
            functionName,
            "miscFlags must not include sharing flags; use syncMode instead");

    if (desc.sampleCount > 1) {
        Require(desc.mipLevels == 1, functionName, "multisample textures must have exactly one mip level");
    }

    switch (desc.syncMode) {
    case D3D11SharedTextureSyncMode::SharedFence:
    case D3D11SharedTextureSyncMode::KeyedMutex:
        break;
    default:
        throw std::invalid_argument(std::string(functionName) + ": unknown sync mode");
    }
}

ComPtr<ID3D11Texture2D> CreateSharedTexture2D(
    ID3D11Device* device,
    const D3D11SharedTexture2DDesc& desc) {

    Require(device != nullptr, "CreateSharedTexture2D", "device is null");
    ValidateSharedTexture2DDesc(desc, "CreateSharedTexture2D");

    D3D11_TEXTURE2D_DESC textureDesc{};
    textureDesc.Width = desc.width;
    textureDesc.Height = desc.height;
    textureDesc.MipLevels = desc.mipLevels;
    textureDesc.ArraySize = desc.arraySize;
    textureDesc.Format = desc.format;
    textureDesc.SampleDesc.Count = desc.sampleCount;
    textureDesc.SampleDesc.Quality = desc.sampleQuality;
    textureDesc.Usage = desc.usage;
    textureDesc.BindFlags = desc.bindFlags;
    textureDesc.CPUAccessFlags = desc.cpuAccessFlags;
    textureDesc.MiscFlags = MakeSharedTextureMiscFlags(desc.syncMode, desc.miscFlags);

    ComPtr<ID3D11Texture2D> texture;
    D3D11CORE_THROW_IF_FAILED(device->CreateTexture2D(&textureDesc, nullptr, &texture));
    return texture;
}

D3D11SharedTexture2D CreateSharedTexture2DWithHandle(
    ID3D11Device* device,
    const D3D11SharedTexture2DDesc& desc,
    LPCWSTR name) {

    D3D11SharedTexture2D result{};
    result.texture = CreateSharedTexture2D(device, desc);
    result.handle = D3D11SharedResource::CreateSharedHandleOwned(result.texture.Get(), name);
    return result;
}

ComPtr<ID3D11Texture2D> OpenSharedTexture2D(
    ID3D11Device* device,
    HANDLE handle) {
    return D3D11SharedResource::OpenSharedTexture2D(device, handle);
}

ComPtr<ID3D11Texture2D> OpenSharedTexture2D(
    ID3D11Device* device,
    const D3D11SharedHandle& handle) {
    if (!handle) {
        throw std::invalid_argument("OpenSharedTexture2D: invalid shared handle");
    }
    return OpenSharedTexture2D(device, handle.Get());
}

} // namespace D3D11CoreLib
