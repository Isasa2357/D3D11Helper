//
// D3D11TextureTransfer.cpp
//

#include <D3D11Helper/D3D11Gpu/D3D11TextureTransfer.hpp>

#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>

#include <stdexcept>
#include <string>
#include <utility>

namespace D3D11CoreLib {
namespace {

[[noreturn]] void ThrowInvalid(const char* functionName, const std::string& message) {
    throw std::invalid_argument(std::string(functionName) + ": " + message);
}

ComPtr<ID3D11Texture2D> GetTexture2DOrThrow(
    const D3D11Resource& resource,
    const char* functionName,
    const char* argumentName) {

    if (!resource.Get()) {
        ThrowInvalid(functionName, std::string(argumentName) + " is null");
    }

    ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = resource.Get()->QueryInterface(IID_PPV_ARGS(texture.GetAddressOf()));
    if (FAILED(hr) || !texture) {
        ThrowInvalid(functionName, std::string(argumentName) + " is not a Texture2D");
    }

    return texture;
}

void ValidateTextureDescForReadback(
    const D3D11_TEXTURE2D_DESC& desc,
    const char* functionName) {

    if (desc.Width == 0 || desc.Height == 0) {
        ThrowInvalid(functionName, "source texture size must be non-zero");
    }

    if (desc.SampleDesc.Count != 1) {
        ThrowInvalid(functionName, "MSAA textures are not supported by D3D11CpuImage readback");
    }

    if (desc.ArraySize != 1) {
        ThrowInvalid(functionName, "Texture2DArray readback is not supported by this overload");
    }

    if (!IsSinglePlaneCpuImageFormat(desc.Format)) {
        ThrowInvalid(functionName, "unsupported source format for D3D11CpuImage readback");
    }
}

void ValidateSourceBox(
    const D3D11_BOX& box,
    const D3D11_TEXTURE2D_DESC& desc,
    const char* functionName) {

    if (box.front != 0 || box.back != 1) {
        ThrowInvalid(functionName, "source box must be a 2D box with front == 0 and back == 1");
    }

    if (box.left >= box.right || box.top >= box.bottom) {
        ThrowInvalid(functionName, "source box must have positive width and height");
    }

    if (box.right > desc.Width || box.bottom > desc.Height) {
        ThrowInvalid(functionName, "source box exceeds source texture bounds");
    }
}

ComPtr<ID3D11Texture2D> CreateStagingTexture(
    ID3D11Device* device,
    UINT width,
    UINT height,
    DXGI_FORMAT format,
    const char* functionName) {

    if (!device) {
        ThrowInvalid(functionName, "D3D11 device is null");
    }

    D3D11_TEXTURE2D_DESC stagingDesc{};
    stagingDesc.Width = width;
    stagingDesc.Height = height;
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = format;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.SampleDesc.Quality = 0;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MiscFlags = 0;

    ComPtr<ID3D11Texture2D> staging;
    D3D11CORE_THROW_IF_FAILED_MSG(
        device->CreateTexture2D(&stagingDesc, nullptr, staging.GetAddressOf()),
        "Create staging Texture2D for readback");

    return staging;
}

D3D11CpuImage MapStagingTextureToCpuImage(
    ID3D11DeviceContext* context,
    ID3D11Texture2D* staging,
    UINT width,
    UINT height,
    DXGI_FORMAT format,
    const char* functionName) {

    if (!context) {
        ThrowInvalid(functionName, "D3D11 device context is null");
    }
    if (!staging) {
        ThrowInvalid(functionName, "staging texture is null");
    }

    const UINT rowBytes = GetPackedRowPitch(width, format);
    D3D11CpuImage image = CreateCpuImage(width, height, format, rowBytes);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    D3D11CORE_THROW_IF_FAILED_MSG(
        context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped),
        "Map staging Texture2D for readback");

    try {
        CopyRows(image.pixels.data(),
                 image.planes.empty() ? 0 : image.planes[0].rowPitch,
                 mapped.pData,
                 mapped.RowPitch,
                 rowBytes,
                 height);
    } catch (...) {
        context->Unmap(staging, 0);
        throw;
    }

    context->Unmap(staging, 0);
    return image;
}

} // namespace

D3D11CpuImage ReadbackTexture2DToCpuImage(
    D3D11Core& core,
    const D3D11Resource& srcTexture) {

    constexpr const char* kFunctionName = "ReadbackTexture2DToCpuImage";

    ComPtr<ID3D11Texture2D> src = GetTexture2DOrThrow(srcTexture, kFunctionName, "srcTexture");

    D3D11_TEXTURE2D_DESC desc{};
    src->GetDesc(&desc);

    ValidateTextureDescForReadback(desc, kFunctionName);

    D3D11_BOX fullBox{};
    fullBox.left = 0;
    fullBox.top = 0;
    fullBox.front = 0;
    fullBox.right = desc.Width;
    fullBox.bottom = desc.Height;
    fullBox.back = 1;

    return ReadbackTexture2DRegionToCpuImage(core, srcTexture, fullBox);
}

D3D11CpuImage ReadbackTexture2DRegionToCpuImage(
    D3D11Core& core,
    const D3D11Resource& srcTexture,
    const D3D11_BOX& srcBox) {

    constexpr const char* kFunctionName = "ReadbackTexture2DRegionToCpuImage";

    ComPtr<ID3D11Texture2D> src = GetTexture2DOrThrow(srcTexture, kFunctionName, "srcTexture");

    D3D11_TEXTURE2D_DESC desc{};
    src->GetDesc(&desc);

    ValidateTextureDescForReadback(desc, kFunctionName);
    ValidateSourceBox(srcBox, desc, kFunctionName);

    const UINT dstWidth = srcBox.right - srcBox.left;
    const UINT dstHeight = srcBox.bottom - srcBox.top;

    ComPtr<ID3D11Texture2D> staging = CreateStagingTexture(
        core.GetDevice(), dstWidth, dstHeight, desc.Format, kFunctionName);

    ID3D11DeviceContext* context = core.GetImmediateContext();
    if (!context) {
        ThrowInvalid(kFunctionName, "immediate context is null");
    }

    context->CopySubresourceRegion(
        staging.Get(),
        0,
        0,
        0,
        0,
        src.Get(),
        0,
        &srcBox);

    // Make the synchronous API deterministic and avoid relying on Map's implicit
    // blocking behavior.
    core.Flush();

    return MapStagingTextureToCpuImage(
        context,
        staging.Get(),
        dstWidth,
        dstHeight,
        desc.Format,
        kFunctionName);
}

D3D11Resource CreateTexture2DFromCpuImage(
    D3D11Core& core,
    const D3D11CpuImage& image,
    UINT bindFlags,
    D3D11_USAGE usage,
    UINT miscFlags) {

    (void)core;
    (void)image;
    (void)bindFlags;
    (void)usage;
    (void)miscFlags;

    throw std::logic_error(
        "CreateTexture2DFromCpuImage is scheduled for the v1.2.0 upload implementation step");
}

void UpdateTexture2DFromCpuImage(
    D3D11Core& core,
    D3D11Resource& dstTexture,
    const D3D11CpuImage& image) {

    (void)core;
    (void)dstTexture;
    (void)image;

    throw std::logic_error(
        "UpdateTexture2DFromCpuImage is scheduled for the v1.2.0 upload implementation step");
}

} // namespace D3D11CoreLib
