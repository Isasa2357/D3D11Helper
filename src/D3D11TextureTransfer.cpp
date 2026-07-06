//
// D3D11TextureTransfer.cpp
//
#include <D3D11Helper/D3D11Gpu/D3D11TextureTransfer.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>
#include <D3D11Helper/D3D11Foundation/D3D11FormatUtil.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>

namespace D3D11CoreLib {
namespace {

[[noreturn]] void ThrowTransferError(const char* functionName, const std::string& message) {
    throw std::invalid_argument(std::string(functionName) + ": " + message);
}

ComPtr<ID3D11Texture2D> AsTexture2DChecked(const D3D11Resource& resource, const char* functionName) {
    if (!resource.Get()) {
        ThrowTransferError(functionName, "resource is null");
    }

    ComPtr<ID3D11Texture2D> texture;
    D3D11CORE_THROW_IF_FAILED_MSG(
        resource.Get()->QueryInterface(IID_PPV_ARGS(&texture)),
        functionName);

    if (!texture) {
        ThrowTransferError(functionName, "resource is not ID3D11Texture2D");
    }
    return texture;
}

void ValidateTextureFormatForCpuTransfer(DXGI_FORMAT format, const char* functionName) {
    if (!IsSinglePlaneCpuImageFormat(format)) {
        ThrowTransferError(functionName, "unsupported format for CPU image transfer");
    }
    if (FormatUtil::IsBlockCompressedFormat(format)) {
        ThrowTransferError(functionName, "block-compressed formats are not supported");
    }
    if (FormatUtil::IsPlanarFormat(format)) {
        ThrowTransferError(functionName, "planar formats are not supported in v1.2.0 TextureTransfer");
    }
}

void ValidateReadableTextureDesc(const D3D11_TEXTURE2D_DESC& desc, const char* functionName) {
    if (desc.Width == 0 || desc.Height == 0) {
        ThrowTransferError(functionName, "texture size must be non-zero");
    }
    if (desc.SampleDesc.Count != 1) {
        ThrowTransferError(functionName, "MSAA textures are not supported");
    }
    ValidateTextureFormatForCpuTransfer(desc.Format, functionName);
}

void ValidateBox2D(const D3D11_BOX& box, const D3D11_TEXTURE2D_DESC& desc, const char* functionName) {
    if (box.left >= box.right || box.top >= box.bottom) {
        ThrowTransferError(functionName, "empty or inverted source box");
    }
    if (box.front != 0 || box.back != 1) {
        ThrowTransferError(functionName, "for Texture2D, box.front must be 0 and box.back must be 1");
    }
    if (box.right > desc.Width || box.bottom > desc.Height) {
        ThrowTransferError(functionName, "source box is outside texture bounds");
    }
}

ComPtr<ID3D11Texture2D> CreateReadbackStagingTexture(
    D3D11Core& core,
    UINT width,
    UINT height,
    DXGI_FORMAT format,
    const char* functionName) {

    D3D11_TEXTURE2D_DESC stagingDesc = {};
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
        core.GetDevice()->CreateTexture2D(&stagingDesc, nullptr, &staging),
        functionName);
    return staging;
}

D3D11CpuImage MapStagingTextureToCpuImage(
    ID3D11DeviceContext* ctx,
    ID3D11Texture2D* staging,
    UINT width,
    UINT height,
    DXGI_FORMAT format,
    const char* functionName) {

    D3D11CpuImage image = CreateCpuImage(width, height, format);

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    D3D11CORE_THROW_IF_FAILED_MSG(
        ctx->Map(staging, 0, D3D11_MAP_READ, 0, &mapped),
        functionName);

    try {
        const UINT rowBytes = GetPackedRowPitch(width, format);
        CopyRows(
            image.pixels.data() + image.planes[0].offsetBytes,
            image.planes[0].rowPitch,
            mapped.pData,
            mapped.RowPitch,
            rowBytes,
            height);
    } catch (...) {
        ctx->Unmap(staging, 0);
        throw;
    }

    ctx->Unmap(staging, 0);
    return image;
}

void ValidateCpuImageForTextureCreate(const D3D11CpuImage& image, D3D11_USAGE usage, const char* functionName) {
    ValidateCpuImage(image, functionName);
    ValidateTextureFormatForCpuTransfer(image.format, functionName);

    if (usage == D3D11_USAGE_STAGING) {
        ThrowTransferError(functionName, "CreateTexture2DFromCpuImage does not create staging textures");
    }
}

void ValidateTextureUpdateTarget(const D3D11_TEXTURE2D_DESC& desc, const D3D11CpuImage& image, const char* functionName) {
    ValidateCpuImage(image, functionName);
    ValidateTextureFormatForCpuTransfer(image.format, functionName);

    if (desc.Width != image.width || desc.Height != image.height) {
        ThrowTransferError(functionName, "texture size does not match image size");
    }
    if (desc.Format != image.format) {
        ThrowTransferError(functionName, "texture format does not match image format");
    }
    if (desc.SampleDesc.Count != 1) {
        ThrowTransferError(functionName, "MSAA textures are not supported");
    }
    if (desc.Usage == D3D11_USAGE_IMMUTABLE) {
        ThrowTransferError(functionName, "immutable textures cannot be updated");
    }
    if (desc.Usage == D3D11_USAGE_STAGING) {
        ThrowTransferError(functionName, "staging texture update is not supported by this helper");
    }
}

const void* FirstPlaneData(const D3D11CpuImage& image) noexcept {
    return image.pixels.data() + image.planes[0].offsetBytes;
}

UINT FirstPlaneRowPitch(const D3D11CpuImage& image) noexcept {
    return image.planes[0].rowPitch;
}

} // namespace

D3D11CpuImage ReadbackTexture2DToCpuImage(
    D3D11Core& core,
    const D3D11Resource& srcTexture) {

    constexpr const char* kFn = "ReadbackTexture2DToCpuImage";
    auto src = AsTexture2DChecked(srcTexture, kFn);

    D3D11_TEXTURE2D_DESC srcDesc = {};
    src->GetDesc(&srcDesc);
    ValidateReadableTextureDesc(srcDesc, kFn);

    auto staging = CreateReadbackStagingTexture(core, srcDesc.Width, srcDesc.Height, srcDesc.Format, kFn);
    auto* ctx = core.GetImmediateContext();

    ctx->CopySubresourceRegion(staging.Get(), 0, 0, 0, 0, src.Get(), 0, nullptr);
    core.Flush();

    return MapStagingTextureToCpuImage(ctx, staging.Get(), srcDesc.Width, srcDesc.Height, srcDesc.Format, kFn);
}

D3D11CpuImage ReadbackTexture2DRegionToCpuImage(
    D3D11Core& core,
    const D3D11Resource& srcTexture,
    const D3D11_BOX& srcBox) {

    constexpr const char* kFn = "ReadbackTexture2DRegionToCpuImage";
    auto src = AsTexture2DChecked(srcTexture, kFn);

    D3D11_TEXTURE2D_DESC srcDesc = {};
    src->GetDesc(&srcDesc);
    ValidateReadableTextureDesc(srcDesc, kFn);
    ValidateBox2D(srcBox, srcDesc, kFn);

    const UINT width = srcBox.right - srcBox.left;
    const UINT height = srcBox.bottom - srcBox.top;

    auto staging = CreateReadbackStagingTexture(core, width, height, srcDesc.Format, kFn);
    auto* ctx = core.GetImmediateContext();

    ctx->CopySubresourceRegion(staging.Get(), 0, 0, 0, 0, src.Get(), 0, &srcBox);
    core.Flush();

    return MapStagingTextureToCpuImage(ctx, staging.Get(), width, height, srcDesc.Format, kFn);
}

D3D11Resource CreateTexture2DFromCpuImage(
    D3D11Core& core,
    const D3D11CpuImage& image,
    UINT bindFlags,
    D3D11_USAGE usage,
    UINT miscFlags) {

    constexpr const char* kFn = "CreateTexture2DFromCpuImage";
    ValidateCpuImageForTextureCreate(image, usage, kFn);

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = image.width;
    desc.Height = image.height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = image.format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = usage;
    desc.BindFlags = bindFlags;
    desc.MiscFlags = miscFlags;

    switch (usage) {
        case D3D11_USAGE_DEFAULT:
        case D3D11_USAGE_IMMUTABLE:
            desc.CPUAccessFlags = 0;
            break;
        case D3D11_USAGE_DYNAMIC:
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            break;
        default:
            ThrowTransferError(kFn, "unsupported texture usage");
    }

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = FirstPlaneData(image);
    init.SysMemPitch = FirstPlaneRowPitch(image);
    init.SysMemSlicePitch = 0;

    ComPtr<ID3D11Texture2D> texture;
    D3D11CORE_THROW_IF_FAILED_MSG(
        core.GetDevice()->CreateTexture2D(&desc, &init, &texture),
        kFn);

    return D3D11Resource(texture);
}

void UpdateTexture2DFromCpuImage(
    D3D11Core& core,
    D3D11Resource& dstTexture,
    const D3D11CpuImage& image) {

    constexpr const char* kFn = "UpdateTexture2DFromCpuImage";
    auto dst = AsTexture2DChecked(dstTexture, kFn);

    D3D11_TEXTURE2D_DESC desc = {};
    dst->GetDesc(&desc);
    ValidateTextureUpdateTarget(desc, image, kFn);

    auto* ctx = core.GetImmediateContext();
    const void* srcData = FirstPlaneData(image);
    const UINT srcRowPitch = FirstPlaneRowPitch(image);

    if (desc.Usage == D3D11_USAGE_DYNAMIC) {
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        D3D11CORE_THROW_IF_FAILED_MSG(
            ctx->Map(dst.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped),
            kFn);

        try {
            const UINT rowBytes = GetPackedRowPitch(image.width, image.format);
            CopyRows(
                mapped.pData,
                mapped.RowPitch,
                srcData,
                srcRowPitch,
                rowBytes,
                image.height);
        } catch (...) {
            ctx->Unmap(dst.Get(), 0);
            throw;
        }

        ctx->Unmap(dst.Get(), 0);
        return;
    }

    // D3D11_USAGE_DEFAULT path.
    ctx->UpdateSubresource(dst.Get(), 0, nullptr, srcData, srcRowPitch, 0);
}

} // namespace D3D11CoreLib
