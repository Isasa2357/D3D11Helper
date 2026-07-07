//
// D3D11View.cpp
//
#include <D3D11Helper/D3D11Gpu/D3D11View.hpp>
#include <D3D11Helper/D3D11Foundation/D3D11FormatUtil.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>

#include <limits>
#include <stdexcept>
#include <string>

namespace D3D11CoreLib {
namespace {

void Require(bool condition, const char* functionName, const char* message) {
    if (!condition) {
        throw std::invalid_argument(std::string(functionName) + ": " + message);
    }
}

void RequireRange(bool condition, const char* functionName, const char* message) {
    if (!condition) {
        throw std::out_of_range(std::string(functionName) + ": " + message);
    }
}

UINT ResolveCount(UINT first, UINT requested, UINT total, const char* functionName, const char* rangeName) {
    RequireRange(first < total, functionName, rangeName);
    const UINT remaining = total - first;
    const UINT count = (requested == UINT(-1)) ? remaining : requested;
    RequireRange(count > 0 && count <= remaining, functionName, rangeName);
    return count;
}

UINT ResolveMipLevels(UINT mostDetailedMip, UINT mipLevels, UINT totalMipLevels, const char* functionName) {
    return ResolveCount(mostDetailedMip, mipLevels, totalMipLevels, functionName, "mip range is out of bounds");
}

DXGI_FORMAT ResolveFormat(DXGI_FORMAT requested, DXGI_FORMAT resourceFormat, const char* functionName) {
    const DXGI_FORMAT result = (requested == DXGI_FORMAT_UNKNOWN) ? resourceFormat : requested;
    Require(result != DXGI_FORMAT_UNKNOWN, functionName, "view format must not be DXGI_FORMAT_UNKNOWN");
    return result;
}

D3D11_TEXTURE2D_DESC GetTextureDesc(ID3D11Texture2D* texture, const char* functionName) {
    Require(texture != nullptr, functionName, "texture is null");
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    Require(desc.MipLevels > 0, functionName, "texture has zero mip levels");
    Require(desc.ArraySize > 0, functionName, "texture has zero array slices");
    return desc;
}

void RequireDeviceAndTexture(ID3D11Device* device, ID3D11Texture2D* texture, const char* functionName) {
    Require(device != nullptr, functionName, "device is null");
    Require(texture != nullptr, functionName, "texture is null");
}

void RequireBindFlag(UINT bindFlags, UINT requiredFlag, const char* functionName, const char* message) {
    Require((bindFlags & requiredFlag) != 0, functionName, message);
}

D3D11BufferViewDesc MakeBufferViewDesc(DXGI_FORMAT format, UINT firstElement, UINT numElements, UINT flags) {
    D3D11BufferViewDesc desc{};
    desc.format = format;
    desc.firstElement = firstElement;
    desc.numElements = numElements;
    desc.flags = flags;
    return desc;
}

D3D11_BUFFER_DESC GetBufferDesc(ID3D11Buffer* buffer, const char* functionName) {
    Require(buffer != nullptr, functionName, "buffer is null");
    D3D11_BUFFER_DESC desc{};
    buffer->GetDesc(&desc);
    Require(desc.ByteWidth > 0, functionName, "buffer byte width is zero");
    return desc;
}

void RequireBufferRange(UINT firstElement, UINT numElements, UINT capacity, const char* functionName) {
    Require(numElements > 0, functionName, "numElements must be greater than zero");
    RequireRange(firstElement <= capacity && numElements <= capacity - firstElement,
                 functionName, "buffer view range is outside buffer bounds");
}

void ValidateTypedBufferView(const D3D11_BUFFER_DESC& desc, DXGI_FORMAT format, UINT firstElement, UINT numElements, const char* functionName) {
    Require(format != DXGI_FORMAT_UNKNOWN, functionName, "typed buffer view requires a typed format");
    Require((desc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED) == 0,
            functionName, "typed buffer view cannot use a structured buffer");
    const UINT bytesPerElement = FormatUtil::BytesPerPixel(format);
    if (bytesPerElement != 0) {
        RequireBufferRange(firstElement, numElements, desc.ByteWidth / bytesPerElement, functionName);
    } else {
        Require(numElements > 0, functionName, "numElements must be greater than zero");
    }
}

void ValidateStructuredBufferView(const D3D11_BUFFER_DESC& desc, UINT firstElement, UINT numElements, const char* functionName) {
    Require((desc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED) != 0,
            functionName, "structured buffer view requires D3D11_RESOURCE_MISC_BUFFER_STRUCTURED");
    Require(desc.StructureByteStride > 0, functionName, "structured buffer stride is zero");
    RequireBufferRange(firstElement, numElements, desc.ByteWidth / desc.StructureByteStride, functionName);
}

void ValidateRawBufferView(const D3D11_BUFFER_DESC& desc, UINT firstElement, UINT numElements, const char* functionName) {
    Require((desc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS) != 0,
            functionName, "raw buffer view requires D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS");
    Require((desc.ByteWidth % 4u) == 0, functionName, "raw buffer byte width must be a multiple of 4");
    RequireBufferRange(firstElement, numElements, desc.ByteWidth / 4u, functionName);
}

void RequireCubeTexture(const D3D11_TEXTURE2D_DESC& desc, const char* functionName) {
    Require((desc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE) != 0,
            functionName, "texture must have D3D11_RESOURCE_MISC_TEXTURECUBE");
    Require((desc.ArraySize % 6u) == 0, functionName, "cube texture array size must be a multiple of 6");
}

} // namespace

ComPtr<ID3D11ShaderResourceView> CreateTexture2DArraySrv(
    ID3D11Device* device,
    ID3D11Texture2D* texture,
    const D3D11Texture2DArrayViewDesc& desc) {
    constexpr const char* kName = "CreateTexture2DArraySrv";
    RequireDeviceAndTexture(device, texture, kName);
    const auto textureDesc = GetTextureDesc(texture, kName);
    RequireBindFlag(textureDesc.BindFlags, D3D11_BIND_SHADER_RESOURCE, kName,
                    "texture must have D3D11_BIND_SHADER_RESOURCE");

    const UINT arraySize = ResolveCount(desc.firstArraySlice, desc.arraySize, textureDesc.ArraySize, kName,
                                        "array slice range is out of bounds");

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = ResolveFormat(desc.format, textureDesc.Format, kName);
    if (textureDesc.SampleDesc.Count > 1) {
        Require(desc.mostDetailedMip == 0, kName, "MSAA Texture2DArray SRV does not support mip selection");
        Require(desc.mipLevels == UINT(-1) || desc.mipLevels == 1, kName,
                "MSAA Texture2DArray SRV does not support mip selection");
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
        srvDesc.Texture2DMSArray.FirstArraySlice = desc.firstArraySlice;
        srvDesc.Texture2DMSArray.ArraySize = arraySize;
    } else {
        const UINT mipLevels = ResolveMipLevels(desc.mostDetailedMip, desc.mipLevels, textureDesc.MipLevels, kName);
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Texture2DArray.MostDetailedMip = desc.mostDetailedMip;
        srvDesc.Texture2DArray.MipLevels = mipLevels;
        srvDesc.Texture2DArray.FirstArraySlice = desc.firstArraySlice;
        srvDesc.Texture2DArray.ArraySize = arraySize;
    }

    ComPtr<ID3D11ShaderResourceView> view;
    D3D11CORE_THROW_IF_FAILED(device->CreateShaderResourceView(texture, &srvDesc, &view));
    return view;
}

ComPtr<ID3D11UnorderedAccessView> CreateTexture2DArrayUav(
    ID3D11Device* device,
    ID3D11Texture2D* texture,
    const D3D11Texture2DArrayViewDesc& desc) {
    constexpr const char* kName = "CreateTexture2DArrayUav";
    RequireDeviceAndTexture(device, texture, kName);
    const auto textureDesc = GetTextureDesc(texture, kName);
    RequireBindFlag(textureDesc.BindFlags, D3D11_BIND_UNORDERED_ACCESS, kName,
                    "texture must have D3D11_BIND_UNORDERED_ACCESS");
    Require(textureDesc.SampleDesc.Count == 1, kName, "UAV cannot be created for multisample Texture2DArray");
    RequireRange(desc.mipSlice < textureDesc.MipLevels, kName, "mip slice is out of range");
    const UINT arraySize = ResolveCount(desc.firstArraySlice, desc.arraySize, textureDesc.ArraySize, kName,
                                        "array slice range is out of bounds");

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format = ResolveFormat(desc.format, textureDesc.Format, kName);
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
    uavDesc.Texture2DArray.MipSlice = desc.mipSlice;
    uavDesc.Texture2DArray.FirstArraySlice = desc.firstArraySlice;
    uavDesc.Texture2DArray.ArraySize = arraySize;

    ComPtr<ID3D11UnorderedAccessView> view;
    D3D11CORE_THROW_IF_FAILED(device->CreateUnorderedAccessView(texture, &uavDesc, &view));
    return view;
}

ComPtr<ID3D11RenderTargetView> CreateTexture2DArrayRtv(
    ID3D11Device* device,
    ID3D11Texture2D* texture,
    const D3D11Texture2DArrayViewDesc& desc) {
    constexpr const char* kName = "CreateTexture2DArrayRtv";
    RequireDeviceAndTexture(device, texture, kName);
    const auto textureDesc = GetTextureDesc(texture, kName);
    RequireBindFlag(textureDesc.BindFlags, D3D11_BIND_RENDER_TARGET, kName,
                    "texture must have D3D11_BIND_RENDER_TARGET");
    const UINT arraySize = ResolveCount(desc.firstArraySlice, desc.arraySize, textureDesc.ArraySize, kName,
                                        "array slice range is out of bounds");

    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = ResolveFormat(desc.format, textureDesc.Format, kName);
    if (textureDesc.SampleDesc.Count > 1) {
        Require(desc.mipSlice == 0, kName, "MSAA Texture2DArray RTV does not support mip selection");
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
        rtvDesc.Texture2DMSArray.FirstArraySlice = desc.firstArraySlice;
        rtvDesc.Texture2DMSArray.ArraySize = arraySize;
    } else {
        RequireRange(desc.mipSlice < textureDesc.MipLevels, kName, "mip slice is out of range");
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        rtvDesc.Texture2DArray.MipSlice = desc.mipSlice;
        rtvDesc.Texture2DArray.FirstArraySlice = desc.firstArraySlice;
        rtvDesc.Texture2DArray.ArraySize = arraySize;
    }

    ComPtr<ID3D11RenderTargetView> view;
    D3D11CORE_THROW_IF_FAILED(device->CreateRenderTargetView(texture, &rtvDesc, &view));
    return view;
}

ComPtr<ID3D11DepthStencilView> CreateTexture2DArrayDsv(
    ID3D11Device* device,
    ID3D11Texture2D* texture,
    const D3D11Texture2DArrayViewDesc& desc) {
    constexpr const char* kName = "CreateTexture2DArrayDsv";
    RequireDeviceAndTexture(device, texture, kName);
    const auto textureDesc = GetTextureDesc(texture, kName);
    RequireBindFlag(textureDesc.BindFlags, D3D11_BIND_DEPTH_STENCIL, kName,
                    "texture must have D3D11_BIND_DEPTH_STENCIL");
    const UINT arraySize = ResolveCount(desc.firstArraySlice, desc.arraySize, textureDesc.ArraySize, kName,
                                        "array slice range is out of bounds");

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = ResolveFormat(desc.format, GetDepthStencilViewFormat(textureDesc.Format), kName);
    if (textureDesc.SampleDesc.Count > 1) {
        Require(desc.mipSlice == 0, kName, "MSAA Texture2DArray DSV does not support mip selection");
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
        dsvDesc.Texture2DMSArray.FirstArraySlice = desc.firstArraySlice;
        dsvDesc.Texture2DMSArray.ArraySize = arraySize;
    } else {
        RequireRange(desc.mipSlice < textureDesc.MipLevels, kName, "mip slice is out of range");
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        dsvDesc.Texture2DArray.MipSlice = desc.mipSlice;
        dsvDesc.Texture2DArray.FirstArraySlice = desc.firstArraySlice;
        dsvDesc.Texture2DArray.ArraySize = arraySize;
    }

    ComPtr<ID3D11DepthStencilView> view;
    D3D11CORE_THROW_IF_FAILED(device->CreateDepthStencilView(texture, &dsvDesc, &view));
    return view;
}

ComPtr<ID3D11ShaderResourceView> CreateTextureCubeSrv(
    ID3D11Device* device,
    ID3D11Texture2D* texture,
    const D3D11TextureCubeViewDesc& desc) {
    constexpr const char* kName = "CreateTextureCubeSrv";
    RequireDeviceAndTexture(device, texture, kName);
    const auto textureDesc = GetTextureDesc(texture, kName);
    RequireBindFlag(textureDesc.BindFlags, D3D11_BIND_SHADER_RESOURCE, kName,
                    "texture must have D3D11_BIND_SHADER_RESOURCE");
    RequireCubeTexture(textureDesc, kName);
    Require(textureDesc.ArraySize == 6, kName, "single cube SRV requires exactly 6 array slices; use CreateTextureCubeArraySrv for cube arrays");
    const UINT mipLevels = ResolveMipLevels(desc.mostDetailedMip, desc.mipLevels, textureDesc.MipLevels, kName);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = ResolveFormat(desc.format, textureDesc.Format, kName);
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MostDetailedMip = desc.mostDetailedMip;
    srvDesc.TextureCube.MipLevels = mipLevels;

    ComPtr<ID3D11ShaderResourceView> view;
    D3D11CORE_THROW_IF_FAILED(device->CreateShaderResourceView(texture, &srvDesc, &view));
    return view;
}

ComPtr<ID3D11ShaderResourceView> CreateTextureCubeArraySrv(
    ID3D11Device* device,
    ID3D11Texture2D* texture,
    const D3D11TextureCubeArrayViewDesc& desc) {
    constexpr const char* kName = "CreateTextureCubeArraySrv";
    RequireDeviceAndTexture(device, texture, kName);
    const auto textureDesc = GetTextureDesc(texture, kName);
    RequireBindFlag(textureDesc.BindFlags, D3D11_BIND_SHADER_RESOURCE, kName,
                    "texture must have D3D11_BIND_SHADER_RESOURCE");
    RequireCubeTexture(textureDesc, kName);
    const UINT totalCubes = textureDesc.ArraySize / 6u;
    const UINT cubeCount = ResolveCount(desc.firstCube, desc.cubeCount, totalCubes, kName,
                                        "cube range is out of bounds");
    const UINT mipLevels = ResolveMipLevels(desc.mostDetailedMip, desc.mipLevels, textureDesc.MipLevels, kName);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = ResolveFormat(desc.format, textureDesc.Format, kName);
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
    srvDesc.TextureCubeArray.MostDetailedMip = desc.mostDetailedMip;
    srvDesc.TextureCubeArray.MipLevels = mipLevels;
    srvDesc.TextureCubeArray.First2DArrayFace = desc.firstCube * 6u;
    srvDesc.TextureCubeArray.NumCubes = cubeCount;

    ComPtr<ID3D11ShaderResourceView> view;
    D3D11CORE_THROW_IF_FAILED(device->CreateShaderResourceView(texture, &srvDesc, &view));
    return view;
}

bool IsDepthStencilViewFormat(DXGI_FORMAT format) noexcept {
    switch (format) {
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        return true;
    default:
        return false;
    }
}

bool IsTypelessDepthTextureFormat(DXGI_FORMAT format) noexcept {
    switch (format) {
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R32G8X24_TYPELESS:
        return true;
    default:
        return false;
    }
}

DXGI_FORMAT GetTypelessDepthTextureFormat(DXGI_FORMAT format) noexcept {
    switch (format) {
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
        return DXGI_FORMAT_R32_TYPELESS;
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        return DXGI_FORMAT_R24G8_TYPELESS;
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
        return DXGI_FORMAT_R16_TYPELESS;
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        return DXGI_FORMAT_R32G8X24_TYPELESS;
    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}

DXGI_FORMAT GetDepthStencilViewFormat(DXGI_FORMAT format) noexcept {
    switch (format) {
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
        return DXGI_FORMAT_D32_FLOAT;
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        return DXGI_FORMAT_D24_UNORM_S8_UINT;
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
        return DXGI_FORMAT_D16_UNORM;
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}

DXGI_FORMAT GetDepthShaderResourceViewFormat(DXGI_FORMAT format) noexcept {
    switch (format) {
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
        return DXGI_FORMAT_R32_FLOAT;
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
        return DXGI_FORMAT_R16_UNORM;
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}

DXGI_FORMAT GetStencilShaderResourceViewFormat(DXGI_FORMAT format) noexcept {
    switch (format) {
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        return DXGI_FORMAT_X24_TYPELESS_G8_UINT;
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}

ComPtr<ID3D11DepthStencilView> CreateDepthTexture2DDsv(
    ID3D11Device* device,
    ID3D11Texture2D* texture,
    UINT mipSlice,
    DXGI_FORMAT format,
    UINT flags) {
    constexpr const char* kName = "CreateDepthTexture2DDsv";
    RequireDeviceAndTexture(device, texture, kName);
    const auto textureDesc = GetTextureDesc(texture, kName);
    RequireBindFlag(textureDesc.BindFlags, D3D11_BIND_DEPTH_STENCIL, kName,
                    "texture must have D3D11_BIND_DEPTH_STENCIL");
    Require(textureDesc.ArraySize == 1, kName, "use CreateTexture2DArrayDsv for array depth textures");

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = ResolveFormat(format, GetDepthStencilViewFormat(textureDesc.Format), kName);
    dsvDesc.Flags = flags;
    if (textureDesc.SampleDesc.Count > 1) {
        Require(mipSlice == 0, kName, "MSAA DSV does not support mip selection");
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
    } else {
        RequireRange(mipSlice < textureDesc.MipLevels, kName, "mip slice is out of range");
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Texture2D.MipSlice = mipSlice;
    }

    ComPtr<ID3D11DepthStencilView> view;
    D3D11CORE_THROW_IF_FAILED(device->CreateDepthStencilView(texture, &dsvDesc, &view));
    return view;
}

ComPtr<ID3D11ShaderResourceView> CreateDepthTexture2DSrv(
    ID3D11Device* device,
    ID3D11Texture2D* texture,
    UINT mostDetailedMip,
    UINT mipLevels,
    DXGI_FORMAT format) {
    constexpr const char* kName = "CreateDepthTexture2DSrv";
    RequireDeviceAndTexture(device, texture, kName);
    const auto textureDesc = GetTextureDesc(texture, kName);
    RequireBindFlag(textureDesc.BindFlags, D3D11_BIND_SHADER_RESOURCE, kName,
                    "texture must have D3D11_BIND_SHADER_RESOURCE");
    Require(textureDesc.ArraySize == 1, kName, "array depth textures are not supported by this helper");

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = ResolveFormat(format, GetDepthShaderResourceViewFormat(textureDesc.Format), kName);
    if (textureDesc.SampleDesc.Count > 1) {
        Require(mostDetailedMip == 0, kName, "MSAA depth SRV does not support mip selection");
        Require(mipLevels == UINT(-1) || mipLevels == 1, kName, "MSAA depth SRV does not support mip selection");
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
    } else {
        const UINT resolvedMipLevels = ResolveMipLevels(mostDetailedMip, mipLevels, textureDesc.MipLevels, kName);
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = mostDetailedMip;
        srvDesc.Texture2D.MipLevels = resolvedMipLevels;
    }

    ComPtr<ID3D11ShaderResourceView> view;
    D3D11CORE_THROW_IF_FAILED(device->CreateShaderResourceView(texture, &srvDesc, &view));
    return view;
}

ComPtr<ID3D11ShaderResourceView> CreateTypedBufferSrv(
    ID3D11Device* device,
    ID3D11Buffer* buffer,
    DXGI_FORMAT format,
    UINT firstElement,
    UINT numElements) {
    constexpr const char* kName = "CreateTypedBufferSrv";
    Require(device != nullptr, kName, "device is null");
    const auto bufferDesc = GetBufferDesc(buffer, kName);
    RequireBindFlag(bufferDesc.BindFlags, D3D11_BIND_SHADER_RESOURCE, kName,
                    "buffer must have D3D11_BIND_SHADER_RESOURCE");
    ValidateTypedBufferView(bufferDesc, format, firstElement, numElements, kName);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = firstElement;
    srvDesc.Buffer.NumElements = numElements;

    ComPtr<ID3D11ShaderResourceView> view;
    D3D11CORE_THROW_IF_FAILED(device->CreateShaderResourceView(buffer, &srvDesc, &view));
    return view;
}

ComPtr<ID3D11UnorderedAccessView> CreateTypedBufferUav(
    ID3D11Device* device,
    ID3D11Buffer* buffer,
    DXGI_FORMAT format,
    UINT firstElement,
    UINT numElements,
    UINT flags) {
    constexpr const char* kName = "CreateTypedBufferUav";
    Require(device != nullptr, kName, "device is null");
    const auto bufferDesc = GetBufferDesc(buffer, kName);
    RequireBindFlag(bufferDesc.BindFlags, D3D11_BIND_UNORDERED_ACCESS, kName,
                    "buffer must have D3D11_BIND_UNORDERED_ACCESS");
    ValidateTypedBufferView(bufferDesc, format, firstElement, numElements, kName);

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format = format;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = firstElement;
    uavDesc.Buffer.NumElements = numElements;
    uavDesc.Buffer.Flags = flags;

    ComPtr<ID3D11UnorderedAccessView> view;
    D3D11CORE_THROW_IF_FAILED(device->CreateUnorderedAccessView(buffer, &uavDesc, &view));
    return view;
}

ComPtr<ID3D11ShaderResourceView> CreateStructuredBufferSrv(
    ID3D11Device* device,
    ID3D11Buffer* buffer,
    UINT firstElement,
    UINT numElements) {
    constexpr const char* kName = "CreateStructuredBufferSrv";
    Require(device != nullptr, kName, "device is null");
    const auto bufferDesc = GetBufferDesc(buffer, kName);
    RequireBindFlag(bufferDesc.BindFlags, D3D11_BIND_SHADER_RESOURCE, kName,
                    "buffer must have D3D11_BIND_SHADER_RESOURCE");
    ValidateStructuredBufferView(bufferDesc, firstElement, numElements, kName);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = firstElement;
    srvDesc.Buffer.NumElements = numElements;

    ComPtr<ID3D11ShaderResourceView> view;
    D3D11CORE_THROW_IF_FAILED(device->CreateShaderResourceView(buffer, &srvDesc, &view));
    return view;
}

ComPtr<ID3D11UnorderedAccessView> CreateStructuredBufferUav(
    ID3D11Device* device,
    ID3D11Buffer* buffer,
    UINT firstElement,
    UINT numElements,
    UINT flags) {
    constexpr const char* kName = "CreateStructuredBufferUav";
    Require(device != nullptr, kName, "device is null");
    const auto bufferDesc = GetBufferDesc(buffer, kName);
    RequireBindFlag(bufferDesc.BindFlags, D3D11_BIND_UNORDERED_ACCESS, kName,
                    "buffer must have D3D11_BIND_UNORDERED_ACCESS");
    ValidateStructuredBufferView(bufferDesc, firstElement, numElements, kName);

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = firstElement;
    uavDesc.Buffer.NumElements = numElements;
    uavDesc.Buffer.Flags = flags;

    ComPtr<ID3D11UnorderedAccessView> view;
    D3D11CORE_THROW_IF_FAILED(device->CreateUnorderedAccessView(buffer, &uavDesc, &view));
    return view;
}

ComPtr<ID3D11ShaderResourceView> CreateRawBufferSrv(
    ID3D11Device* device,
    ID3D11Buffer* buffer,
    UINT firstElement,
    UINT numElements) {
    constexpr const char* kName = "CreateRawBufferSrv";
    Require(device != nullptr, kName, "device is null");
    const auto bufferDesc = GetBufferDesc(buffer, kName);
    RequireBindFlag(bufferDesc.BindFlags, D3D11_BIND_SHADER_RESOURCE, kName,
                    "buffer must have D3D11_BIND_SHADER_RESOURCE");
    ValidateRawBufferView(bufferDesc, firstElement, numElements, kName);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
    srvDesc.BufferEx.FirstElement = firstElement;
    srvDesc.BufferEx.NumElements = numElements;
    srvDesc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;

    ComPtr<ID3D11ShaderResourceView> view;
    D3D11CORE_THROW_IF_FAILED(device->CreateShaderResourceView(buffer, &srvDesc, &view));
    return view;
}

ComPtr<ID3D11UnorderedAccessView> CreateRawBufferUav(
    ID3D11Device* device,
    ID3D11Buffer* buffer,
    UINT firstElement,
    UINT numElements,
    UINT flags) {
    constexpr const char* kName = "CreateRawBufferUav";
    Require(device != nullptr, kName, "device is null");
    const auto bufferDesc = GetBufferDesc(buffer, kName);
    RequireBindFlag(bufferDesc.BindFlags, D3D11_BIND_UNORDERED_ACCESS, kName,
                    "buffer must have D3D11_BIND_UNORDERED_ACCESS");
    ValidateRawBufferView(bufferDesc, firstElement, numElements, kName);
    Require((flags & D3D11_BUFFER_UAV_FLAG_RAW) != 0, kName,
            "raw UAV flags must include D3D11_BUFFER_UAV_FLAG_RAW");

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = firstElement;
    uavDesc.Buffer.NumElements = numElements;
    uavDesc.Buffer.Flags = flags;

    ComPtr<ID3D11UnorderedAccessView> view;
    D3D11CORE_THROW_IF_FAILED(device->CreateUnorderedAccessView(buffer, &uavDesc, &view));
    return view;
}

} // namespace D3D11CoreLib
