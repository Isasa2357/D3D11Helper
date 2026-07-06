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

const D3D11CpuImagePlane& GetSinglePlaneOrThrow(
    const D3D11CpuImage& image,
    const char* functionName) {

    ValidateCpuImage(image, functionName);
    if (image.planes.empty()) {
        ThrowInvalid(functionName, "image has no planes");
    }
    return image.planes[0];
}

const uint8_t* GetPlaneDataOrThrow(
    const D3D11CpuImage& image,
    const D3D11CpuImagePlane& plane,
    const char* functionName) {

    if (plane.offsetBytes > image.SizeBytes()) {
        ThrowInvalid(functionName, "plane offset exceeds pixel buffer size");
    }
    return image.pixels.data() + static_cast<size_t>(plane.offsetBytes);
}

void ValidateTextureDescForUpdateTarget(
    const D3D11_TEXTURE2D_DESC& desc,
    const D3D11CpuImage& image,
    const char* functionName) {

    if (desc.Width != image.width || desc.Height != image.height) {
        ThrowInvalid(functionName, "destination texture size does not match CPU image");
    }
    if (desc.Format != image.format) {
        ThrowInvalid(functionName, "destination texture format does not match CPU image");
    }
    if (desc.SampleDesc.Count != 1) {
        ThrowInvalid(functionName, "MSAA textures are not supported by D3D11CpuImage upload");
    }
    if (desc.ArraySize != 1) {
        ThrowInvalid(functionName, "Texture2DArray upload is not supported by this overload");
    }
    if (!IsSinglePlaneCpuImageFormat(desc.Format)) {
        ThrowInvalid(functionName, "unsupported destination format for D3D11CpuImage upload");
    }
}

UINT CpuAccessFlagsForCreateUsage(D3D11_USAGE usage, const char* functionName) {
    switch (usage) {
        case D3D11_USAGE_DEFAULT:
        case D3D11_USAGE_IMMUTABLE:
            return 0;
        case D3D11_USAGE_DYNAMIC:
            return D3D11_CPU_ACCESS_WRITE;
        case D3D11_USAGE_STAGING:
            ThrowInvalid(functionName, "CreateTexture2DFromCpuImage does not create staging textures");
            break;
        default:
            break;
    }

    ThrowInvalid(functionName, "unsupported D3D11_USAGE for CPU image texture creation");
}

void UpdateDefaultTextureFromCpuImage(
    ID3D11DeviceContext* context,
    ID3D11Texture2D* dst,
    const D3D11CpuImage& image,
    const D3D11CpuImagePlane& plane,
    const char* functionName) {

    if (!context) {
        ThrowInvalid(functionName, "D3D11 device context is null");
    }

    const uint8_t* srcData = GetPlaneDataOrThrow(image, plane, functionName);
    context->UpdateSubresource(
        dst,
        0,
        nullptr,
        srcData,
        plane.rowPitch,
        plane.rowPitch * image.height);
}

void UpdateDynamicTextureFromCpuImage(
    ID3D11DeviceContext* context,
    ID3D11Texture2D* dst,
    const D3D11CpuImage& image,
    const D3D11CpuImagePlane& plane,
    const char* functionName) {

    if (!context) {
        ThrowInvalid(functionName, "D3D11 device context is null");
    }

    const uint8_t* srcData = GetPlaneDataOrThrow(image, plane, functionName);
    const UINT rowBytes = GetPackedRowPitch(image.width, image.format);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    D3D11CORE_THROW_IF_FAILED_MSG(
        context->Map(dst, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped),
        "Map dynamic Texture2D for CPU image upload");

    try {
        CopyRows(mapped.pData,
                 mapped.RowPitch,
                 srcData,
                 plane.rowPitch,
                 rowBytes,
                 image.height);
    } catch (...) {
        context->Unmap(dst, 0);
        throw;
    }

    context->Unmap(dst, 0);
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

    constexpr const char* kFunctionName = "CreateTexture2DFromCpuImage";

    const D3D11CpuImagePlane& plane = GetSinglePlaneOrThrow(image, kFunctionName);
    const uint8_t* srcData = GetPlaneDataOrThrow(image, plane, kFunctionName);

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = image.width;
    desc.Height = image.height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = image.format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = usage;
    desc.BindFlags = bindFlags;
    desc.CPUAccessFlags = CpuAccessFlagsForCreateUsage(usage, kFunctionName);
    desc.MiscFlags = miscFlags;

    D3D11_SUBRESOURCE_DATA initialData{};
    initialData.pSysMem = srcData;
    initialData.SysMemPitch = plane.rowPitch;
    initialData.SysMemSlicePitch = plane.rowPitch * image.height;

    ComPtr<ID3D11Texture2D> texture;
    D3D11CORE_THROW_IF_FAILED_MSG(
        core.GetDevice()->CreateTexture2D(&desc, &initialData, texture.GetAddressOf()),
        "Create Texture2D from D3D11CpuImage");

    return D3D11Resource(texture);
}

void UpdateTexture2DFromCpuImage(
    D3D11Core& core,
    D3D11Resource& dstTexture,
    const D3D11CpuImage& image) {

    constexpr const char* kFunctionName = "UpdateTexture2DFromCpuImage";

    const D3D11CpuImagePlane& plane = GetSinglePlaneOrThrow(image, kFunctionName);
    ComPtr<ID3D11Texture2D> dst = GetTexture2DOrThrow(dstTexture, kFunctionName, "dstTexture");

    D3D11_TEXTURE2D_DESC desc{};
    dst->GetDesc(&desc);
    ValidateTextureDescForUpdateTarget(desc, image, kFunctionName);

    switch (desc.Usage) {
        case D3D11_USAGE_DEFAULT:
            UpdateDefaultTextureFromCpuImage(core.GetImmediateContext(), dst.Get(), image, plane, kFunctionName);
            return;
        case D3D11_USAGE_DYNAMIC:
            if ((desc.CPUAccessFlags & D3D11_CPU_ACCESS_WRITE) == 0) {
                ThrowInvalid(kFunctionName, "dynamic destination texture lacks CPU write access");
            }
            UpdateDynamicTextureFromCpuImage(core.GetImmediateContext(), dst.Get(), image, plane, kFunctionName);
            return;
        case D3D11_USAGE_IMMUTABLE:
            ThrowInvalid(kFunctionName, "immutable textures cannot be updated");
            break;
        case D3D11_USAGE_STAGING:
            ThrowInvalid(kFunctionName, "staging texture update is not supported by this helper");
            break;
        default:
            break;
    }

    ThrowInvalid(kFunctionName, "unsupported D3D11_USAGE for CPU image update");
}

} // namespace D3D11CoreLib
