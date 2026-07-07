//
// D3D11Resolve.cpp
//
#include <D3D11Helper/D3D11Gpu/D3D11Resolve.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Copy.hpp>

#include <stdexcept>
#include <string>

namespace D3D11CoreLib {
namespace {

void Require(bool condition, const char* functionName, const char* message) {
    if (!condition) throw std::invalid_argument(std::string(functionName) + ": " + message);
}

void RequireRange(bool condition, const char* functionName, const char* message) {
    if (!condition) throw std::out_of_range(std::string(functionName) + ": " + message);
}

UINT MipExtent(UINT extent, UINT mip) {
    const UINT shifted = extent >> mip;
    return shifted == 0 ? 1u : shifted;
}

} // namespace

void ResolveSubresource(ID3D11DeviceContext* context, ID3D11Resource* dst, UINT dstSubresource, ID3D11Resource* src, UINT srcSubresource, DXGI_FORMAT format) {
    Require(context != nullptr, "ResolveSubresource", "context is null");
    Require(dst != nullptr, "ResolveSubresource", "destination resource is null");
    Require(src != nullptr, "ResolveSubresource", "source resource is null");
    Require(format != DXGI_FORMAT_UNKNOWN, "ResolveSubresource", "resolve format must not be DXGI_FORMAT_UNKNOWN");
    context->ResolveSubresource(dst, dstSubresource, src, srcSubresource, format);
}

void ResolveTexture2D(ID3D11DeviceContext* context, ID3D11Texture2D* dst, ID3D11Texture2D* src, const D3D11ResolveTexture2DDesc& desc) {
    Require(context != nullptr, "ResolveTexture2D", "context is null");
    Require(dst != nullptr, "ResolveTexture2D", "destination texture is null");
    Require(src != nullptr, "ResolveTexture2D", "source texture is null");

    D3D11_TEXTURE2D_DESC dstDesc{};
    D3D11_TEXTURE2D_DESC srcDesc{};
    dst->GetDesc(&dstDesc);
    src->GetDesc(&srcDesc);

    Require(srcDesc.SampleDesc.Count > 1, "ResolveTexture2D", "source texture must be multisampled");
    Require(dstDesc.SampleDesc.Count == 1, "ResolveTexture2D", "destination texture must be single-sample");
    RequireRange(desc.srcMip < srcDesc.MipLevels, "ResolveTexture2D", "source mip index is out of range");
    RequireRange(desc.dstMip < dstDesc.MipLevels, "ResolveTexture2D", "destination mip index is out of range");
    RequireRange(desc.srcArraySlice < srcDesc.ArraySize, "ResolveTexture2D", "source array slice index is out of range");
    RequireRange(desc.dstArraySlice < dstDesc.ArraySize, "ResolveTexture2D", "destination array slice index is out of range");

    const UINT srcWidth = MipExtent(srcDesc.Width, desc.srcMip);
    const UINT srcHeight = MipExtent(srcDesc.Height, desc.srcMip);
    const UINT dstWidth = MipExtent(dstDesc.Width, desc.dstMip);
    const UINT dstHeight = MipExtent(dstDesc.Height, desc.dstMip);
    Require(srcWidth == dstWidth && srcHeight == dstHeight, "ResolveTexture2D", "selected source and destination subresource dimensions must match");

    const DXGI_FORMAT format = desc.format == DXGI_FORMAT_UNKNOWN ? dstDesc.Format : desc.format;
    Require(format != DXGI_FORMAT_UNKNOWN, "ResolveTexture2D", "resolve format must not be DXGI_FORMAT_UNKNOWN");

    ResolveSubresource(context, dst, CalcD3D11Subresource(desc.dstMip, desc.dstArraySlice, dstDesc.MipLevels), src, CalcD3D11Subresource(desc.srcMip, desc.srcArraySlice, srcDesc.MipLevels), format);
}

} // namespace D3D11CoreLib
