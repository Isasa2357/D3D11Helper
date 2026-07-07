//
// D3D11Copy.cpp
//
#include <D3D11Helper/D3D11Gpu/D3D11Copy.hpp>

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

UINT MipExtent(UINT extent, UINT mip) {
    const UINT shifted = extent >> mip;
    return shifted == 0 ? 1u : shifted;
}

void ValidateTextureSubresource(
    ID3D11Texture2D* texture,
    UINT mip,
    UINT arraySlice,
    const char* functionName) {
    Require(texture != nullptr, functionName, "texture is null");

    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    RequireRange(mip < desc.MipLevels, functionName, "mip index is out of range");
    RequireRange(arraySlice < desc.ArraySize, functionName, "array slice index is out of range");
}

} // namespace

D3D11_BOX MakeD3D11Box(const D3D11Box3D& box) {
    D3D11_BOX result{};
    result.left = box.left;
    result.top = box.top;
    result.front = box.front;
    result.right = box.right;
    result.bottom = box.bottom;
    result.back = box.back;
    return result;
}

UINT CalcD3D11Subresource(UINT mipSlice, UINT arraySlice, UINT mipLevels) {
    if (mipLevels == 0) {
        throw std::invalid_argument("CalcD3D11Subresource: mipLevels must be greater than zero");
    }
    if (mipSlice >= mipLevels) {
        throw std::out_of_range("CalcD3D11Subresource: mipSlice is out of range");
    }
    if (arraySlice > (std::numeric_limits<UINT>::max() - mipSlice) / mipLevels) {
        throw std::out_of_range("CalcD3D11Subresource: subresource index overflows UINT");
    }
    return mipSlice + arraySlice * mipLevels;
}

void CopyResource(ID3D11DeviceContext* context, ID3D11Resource* dst, ID3D11Resource* src) {
    Require(context != nullptr, "CopyResource", "context is null");
    Require(dst != nullptr, "CopyResource", "destination resource is null");
    Require(src != nullptr, "CopyResource", "source resource is null");
    context->CopyResource(dst, src);
}

void CopySubresource(
    ID3D11DeviceContext* context,
    ID3D11Resource* dst,
    UINT dstSubresource,
    UINT dstX,
    UINT dstY,
    UINT dstZ,
    ID3D11Resource* src,
    UINT srcSubresource,
    const D3D11_BOX* srcBox) {
    Require(context != nullptr, "CopySubresource", "context is null");
    Require(dst != nullptr, "CopySubresource", "destination resource is null");
    Require(src != nullptr, "CopySubresource", "source resource is null");
    context->CopySubresourceRegion(dst, dstSubresource, dstX, dstY, dstZ, src, srcSubresource, srcBox);
}

void CopyTexture2D(ID3D11DeviceContext* context, ID3D11Texture2D* dst, ID3D11Texture2D* src) {
    Require(dst != nullptr, "CopyTexture2D", "destination texture is null");
    Require(src != nullptr, "CopyTexture2D", "source texture is null");
    CopyResource(context, dst, src);
}

void CopyTexture2DSubresource(
    ID3D11DeviceContext* context,
    ID3D11Texture2D* dst,
    UINT dstMip,
    UINT dstArraySlice,
    ID3D11Texture2D* src,
    UINT srcMip,
    UINT srcArraySlice) {
    Require(context != nullptr, "CopyTexture2DSubresource", "context is null");
    ValidateTextureSubresource(dst, dstMip, dstArraySlice, "CopyTexture2DSubresource");
    ValidateTextureSubresource(src, srcMip, srcArraySlice, "CopyTexture2DSubresource");

    D3D11_TEXTURE2D_DESC dstDesc{};
    D3D11_TEXTURE2D_DESC srcDesc{};
    dst->GetDesc(&dstDesc);
    src->GetDesc(&srcDesc);

    CopySubresource(
        context,
        dst,
        CalcD3D11Subresource(dstMip, dstArraySlice, dstDesc.MipLevels),
        0,
        0,
        0,
        src,
        CalcD3D11Subresource(srcMip, srcArraySlice, srcDesc.MipLevels),
        nullptr);
}

void CopyTexture2DRegion(
    ID3D11DeviceContext* context,
    ID3D11Texture2D* dst,
    ID3D11Texture2D* src,
    const D3D11Texture2DRegion& region) {
    Require(context != nullptr, "CopyTexture2DRegion", "context is null");
    ValidateTextureSubresource(dst, region.dstMip, region.dstArraySlice, "CopyTexture2DRegion");
    ValidateTextureSubresource(src, region.srcMip, region.srcArraySlice, "CopyTexture2DRegion");
    Require(region.width > 0, "CopyTexture2DRegion", "region width must be greater than zero");
    Require(region.height > 0, "CopyTexture2DRegion", "region height must be greater than zero");

    D3D11_TEXTURE2D_DESC dstDesc{};
    D3D11_TEXTURE2D_DESC srcDesc{};
    dst->GetDesc(&dstDesc);
    src->GetDesc(&srcDesc);
    Require(srcDesc.SampleDesc.Count == 1, "CopyTexture2DRegion", "source texture must be single-sample for region copies");
    Require(dstDesc.SampleDesc.Count == 1, "CopyTexture2DRegion", "destination texture must be single-sample for region copies");

    const UINT srcWidth = MipExtent(srcDesc.Width, region.srcMip);
    const UINT srcHeight = MipExtent(srcDesc.Height, region.srcMip);
    const UINT dstWidth = MipExtent(dstDesc.Width, region.dstMip);
    const UINT dstHeight = MipExtent(dstDesc.Height, region.dstMip);
    Require(region.srcX <= srcWidth && region.width <= srcWidth - region.srcX,
            "CopyTexture2DRegion", "source region is outside texture bounds");
    Require(region.srcY <= srcHeight && region.height <= srcHeight - region.srcY,
            "CopyTexture2DRegion", "source region is outside texture bounds");
    Require(region.dstX <= dstWidth && region.width <= dstWidth - region.dstX,
            "CopyTexture2DRegion", "destination region is outside texture bounds");
    Require(region.dstY <= dstHeight && region.height <= dstHeight - region.dstY,
            "CopyTexture2DRegion", "destination region is outside texture bounds");

    D3D11_BOX box{};
    box.left = region.srcX;
    box.top = region.srcY;
    box.front = 0;
    box.right = region.srcX + region.width;
    box.bottom = region.srcY + region.height;
    box.back = 1;

    CopySubresource(
        context,
        dst,
        CalcD3D11Subresource(region.dstMip, region.dstArraySlice, dstDesc.MipLevels),
        region.dstX,
        region.dstY,
        0,
        src,
        CalcD3D11Subresource(region.srcMip, region.srcArraySlice, srcDesc.MipLevels),
        &box);
}

void CopyBuffer(ID3D11DeviceContext* context, ID3D11Buffer* dst, ID3D11Buffer* src) {
    Require(dst != nullptr, "CopyBuffer", "destination buffer is null");
    Require(src != nullptr, "CopyBuffer", "source buffer is null");
    CopyResource(context, dst, src);
}

void CopyBufferRegion(
    ID3D11DeviceContext* context,
    ID3D11Buffer* dst,
    ID3D11Buffer* src,
    const D3D11BufferCopyRegion& region) {
    Require(context != nullptr, "CopyBufferRegion", "context is null");
    Require(dst != nullptr, "CopyBufferRegion", "destination buffer is null");
    Require(src != nullptr, "CopyBufferRegion", "source buffer is null");
    Require(region.sizeBytes > 0, "CopyBufferRegion", "sizeBytes must be greater than zero");

    D3D11_BUFFER_DESC dstDesc{};
    D3D11_BUFFER_DESC srcDesc{};
    dst->GetDesc(&dstDesc);
    src->GetDesc(&srcDesc);
    Require(region.srcOffset <= srcDesc.ByteWidth && region.sizeBytes <= srcDesc.ByteWidth - region.srcOffset,
            "CopyBufferRegion", "source byte range is outside buffer bounds");
    Require(region.dstOffset <= dstDesc.ByteWidth && region.sizeBytes <= dstDesc.ByteWidth - region.dstOffset,
            "CopyBufferRegion", "destination byte range is outside buffer bounds");

    D3D11_BOX box{};
    box.left = region.srcOffset;
    box.right = region.srcOffset + region.sizeBytes;
    box.top = 0;
    box.bottom = 1;
    box.front = 0;
    box.back = 1;
    CopySubresource(context, dst, 0, region.dstOffset, 0, 0, src, 0, &box);
}

} // namespace D3D11CoreLib
