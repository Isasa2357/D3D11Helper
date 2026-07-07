//
// D3D11Mipmap.cpp
//
#include <D3D11Helper/D3D11Gpu/D3D11Mipmap.hpp>
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

bool HasMipGenerationResourceFlags(const D3D11_TEXTURE2D_DESC& desc) noexcept {
    return (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0 &&
           (desc.BindFlags & D3D11_BIND_RENDER_TARGET) != 0 &&
           (desc.MiscFlags & D3D11_RESOURCE_MISC_GENERATE_MIPS) != 0 &&
           desc.MipLevels > 1 &&
           desc.SampleDesc.Count == 1;
}

} // namespace

bool IsAutoGenerateMipsSupported(ID3D11Device* device, DXGI_FORMAT format) {
    Require(device != nullptr, "IsAutoGenerateMipsSupported", "device is null");
    if (format == DXGI_FORMAT_UNKNOWN) {
        return false;
    }

    UINT support = 0;
    if (FAILED(device->CheckFormatSupport(format, &support))) {
        return false;
    }
    return (support & D3D11_FORMAT_SUPPORT_MIP_AUTOGEN) != 0;
}

bool CanGenerateMipsForTexture2D(ID3D11Device* device, ID3D11Texture2D* texture) {
    Require(device != nullptr, "CanGenerateMipsForTexture2D", "device is null");
    Require(texture != nullptr, "CanGenerateMipsForTexture2D", "texture is null");

    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    if (!HasMipGenerationResourceFlags(desc)) {
        return false;
    }
    return IsAutoGenerateMipsSupported(device, desc.Format);
}

ComPtr<ID3D11ShaderResourceView> CreateMipGenerationTexture2DSRV(
    ID3D11Device* device,
    ID3D11Texture2D* texture,
    const D3D11MipGenerationSrvDesc& desc) {
    Require(device != nullptr, "CreateMipGenerationTexture2DSRV", "device is null");
    Require(texture != nullptr, "CreateMipGenerationTexture2DSRV", "texture is null");

    D3D11_TEXTURE2D_DESC textureDesc{};
    texture->GetDesc(&textureDesc);
    Require(HasMipGenerationResourceFlags(textureDesc),
            "CreateMipGenerationTexture2DSRV",
            "texture must be single-sample, have more than one mip, and include SRV/RTV/GENERATE_MIPS flags");
    Require(desc.mostDetailedMip < textureDesc.MipLevels,
            "CreateMipGenerationTexture2DSRV", "mostDetailedMip is out of range");
    if (desc.mipLevels != UINT(-1)) {
        Require(desc.mipLevels > 0,
                "CreateMipGenerationTexture2DSRV", "mipLevels must be greater than zero or UINT(-1)");
        Require(desc.mipLevels <= textureDesc.MipLevels - desc.mostDetailedMip,
                "CreateMipGenerationTexture2DSRV", "mipLevels extends beyond texture mip chain");
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = desc.format == DXGI_FORMAT_UNKNOWN ? textureDesc.Format : desc.format;
    Require(srvDesc.Format != DXGI_FORMAT_UNKNOWN,
            "CreateMipGenerationTexture2DSRV", "SRV format must not be DXGI_FORMAT_UNKNOWN");
    Require(IsAutoGenerateMipsSupported(device, srvDesc.Format),
            "CreateMipGenerationTexture2DSRV", "SRV format does not support automatic mip generation");
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = desc.mostDetailedMip;
    srvDesc.Texture2D.MipLevels = desc.mipLevels;

    ComPtr<ID3D11ShaderResourceView> srv;
    D3D11CORE_THROW_IF_FAILED(device->CreateShaderResourceView(texture, &srvDesc, &srv));
    return srv;
}

void GenerateMips(ID3D11DeviceContext* context, ID3D11ShaderResourceView* srv) {
    Require(context != nullptr, "GenerateMips", "context is null");
    Require(srv != nullptr, "GenerateMips", "shader resource view is null");
    context->GenerateMips(srv);
}

} // namespace D3D11CoreLib
