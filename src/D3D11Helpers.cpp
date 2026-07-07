//
// D3D11Helpers.cpp
//
#include <D3D11Helper/D3D11Gpu/D3D11Helpers.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace D3D11CoreLib {

namespace {

UINT AlignUp(UINT value, UINT alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

UINT BytesPerPixelChecked(DXGI_FORMAT format) {
    if (FormatUtil::IsPlanarFormat(format)) {
        throw std::runtime_error(
            "D3D11Helpers: planar formats require a dedicated upload path");
    }
    const UINT bpp = FormatUtil::BytesPerPixel(format);
    if (bpp == 0) {
        throw std::runtime_error(
            "D3D11Helpers: unsupported / block-compressed format for from-memory creation");
    }
    return bpp;
}

UINT MipSize(UINT size, UINT mip) {
    return std::max<UINT>(1u, size >> mip);
}

UINT GetTextureSubresourceCount(const D3D11_TEXTURE2D_DESC& desc) {
    if (desc.MipLevels == 0 || desc.ArraySize == 0) {
        throw std::runtime_error("D3D11Helpers: invalid Texture2D subresource count");
    }
    return desc.MipLevels * desc.ArraySize;
}

void ValidateSubresourceRange(const D3D11_TEXTURE2D_DESC& desc,
                              UINT firstSubresource,
                              UINT subresourceCount) {
    if (subresourceCount == 0) {
        throw std::runtime_error("D3D11Helpers: subresourceCount must be > 0");
    }
    const UINT total = GetTextureSubresourceCount(desc);
    if (firstSubresource >= total || subresourceCount > total - firstSubresource) {
        throw std::runtime_error("D3D11Helpers: subresource range is out of bounds");
    }
}

void ValidateNonPlanarTextureFormat(DXGI_FORMAT format, const char* name) {
    if (FormatUtil::IsPlanarFormat(format)) {
        throw std::runtime_error(std::string(name) +
            ": planar formats require a dedicated upload path");
    }
}

void ValidateSingleSubresourceTexture(const D3D11_TEXTURE2D_DESC& desc, const char* name) {
    if (desc.MipLevels != 1 || desc.ArraySize != 1) {
        throw std::runtime_error(std::string(name) +
            ": texture has multiple subresources; use UpdateTextureSubresources");
    }
    ValidateNonPlanarTextureFormat(desc.Format, name);
}

UINT DefaultRowPitchForSubresource(UINT width, DXGI_FORMAT format, UINT mip) {
    const UINT bpp = BytesPerPixelChecked(format);
    return MipSize(width, mip) * bpp;
}

UINT DefaultSlicePitchForSubresource(UINT width, UINT height, DXGI_FORMAT format, UINT mip) {
    return DefaultRowPitchForSubresource(width, format, mip) * MipSize(height, mip);
}

D3D11_SUBRESOURCE_DATA MakeSubresourceData(
    const D3D11TextureSubresourceData& src,
    UINT width,
    UINT height,
    DXGI_FORMAT format,
    UINT mip,
    const char* name) {

    if (!src.data) {
        throw std::runtime_error(std::string(name) + ": null subresource data");
    }

    const UINT rowPitch = (src.rowPitch != 0)
        ? src.rowPitch
        : DefaultRowPitchForSubresource(width, format, mip);
    const UINT rowSize = DefaultRowPitchForSubresource(width, format, mip);
    if (rowPitch < rowSize) {
        throw std::runtime_error(std::string(name) + ": rowPitch is smaller than row size");
    }

    const UINT slicePitch = (src.slicePitch != 0)
        ? src.slicePitch
        : rowPitch * MipSize(height, mip);
    if (slicePitch < rowPitch * MipSize(height, mip)) {
        throw std::runtime_error(std::string(name) + ": slicePitch is too small");
    }

    D3D11_SUBRESOURCE_DATA srd = {};
    srd.pSysMem = src.data;
    srd.SysMemPitch = rowPitch;
    srd.SysMemSlicePitch = slicePitch;
    return srd;
}

void CopyRowsToMappedSubresource(const D3D11_MAPPED_SUBRESOURCE& mapped,
                                 const D3D11TextureSubresourceData& src,
                                 UINT width,
                                 UINT height,
                                 DXGI_FORMAT format,
                                 UINT mip,
                                 const char* name) {
    if (!src.data) {
        throw std::runtime_error(std::string(name) + ": null subresource data");
    }

    const UINT rowSize = DefaultRowPitchForSubresource(width, format, mip);
    const UINT rowPitch = (src.rowPitch != 0)
        ? src.rowPitch
        : rowSize;
    if (rowPitch < rowSize) {
        throw std::runtime_error(std::string(name) + ": rowPitch is smaller than row size");
    }

    const auto* srcBytes = static_cast<const uint8_t*>(src.data);
    auto* dstBytes = static_cast<uint8_t*>(mapped.pData);
    const UINT rows = MipSize(height, mip);

    for (UINT y = 0; y < rows; ++y) {
        std::memcpy(dstBytes + static_cast<size_t>(y) * mapped.RowPitch,
                    srcBytes + static_cast<size_t>(y) * rowPitch,
                    rowSize);
    }
}

void CheckBufferResource(const D3D11Resource& buffer, const char* name) {
    if (!buffer) {
        throw std::runtime_error(std::string(name) + ": null buffer");
    }
    const D3D11_BUFFER_DESC desc = buffer.GetBufferDesc();
    if (desc.ByteWidth == 0) {
        throw std::runtime_error(std::string(name) + ": resource is not a buffer");
    }
}

void CheckTexture2DResource(const D3D11Resource& texture, const char* name) {
    if (!texture) {
        throw std::runtime_error(std::string(name) + ": null texture");
    }
    const D3D11_TEXTURE2D_DESC desc = texture.GetTexture2DDesc();
    if (desc.Width == 0 || desc.Height == 0) {
        throw std::runtime_error(std::string(name) + ": resource is not a Texture2D");
    }
}

UINT BytesPerElementForTypedBuffer(DXGI_FORMAT format, const char* name) {
    const UINT bytes = FormatUtil::BytesPerPixel(format);
    if (bytes == 0) {
        throw std::runtime_error(std::string(name) + ": unsupported typed buffer format");
    }
    return bytes;
}

void ValidateBufferView(const D3D11Resource& buffer,
                        UINT firstElement,
                        UINT numElements,
                        DXGI_FORMAT format,
                        const char* name) {
    CheckBufferResource(buffer, name);
    if (numElements == 0) {
        throw std::runtime_error(std::string(name) + ": numElements must be > 0");
    }

    const D3D11_BUFFER_DESC desc = buffer.GetBufferDesc();
    const UINT64 lastElement = static_cast<UINT64>(firstElement) + numElements;

    if (format == DXGI_FORMAT_UNKNOWN) {
        if (!(desc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED)) {
            throw std::runtime_error(std::string(name) +
                ": structured buffer view requires D3D11_RESOURCE_MISC_BUFFER_STRUCTURED");
        }
        if (desc.StructureByteStride == 0) {
            throw std::runtime_error(std::string(name) +
                ": structured buffer has zero StructureByteStride");
        }
        if (lastElement > desc.ByteWidth / desc.StructureByteStride) {
            throw std::runtime_error(std::string(name) + ": structured buffer view exceeds buffer size");
        }
        return;
    }

    if (desc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED) {
        throw std::runtime_error(std::string(name) +
            ": typed buffer view cannot be created from a structured buffer");
    }
    const UINT bytes = BytesPerElementForTypedBuffer(format, name);
    if (lastElement > desc.ByteWidth / bytes) {
        throw std::runtime_error(std::string(name) + ": typed buffer view exceeds buffer size");
    }
}

} // unnamed namespace

// ==========================================================================
// バッファ生成
// ==========================================================================

D3D11Resource CreateBuffer(D3D11Core& core, UINT sizeBytes,
                            D3D11_USAGE usage, UINT bindFlags,
                            UINT cpuAccessFlags, UINT miscFlags,
                            const void* initialData,
                            UINT structureByteStride) {
    if (sizeBytes == 0) throw std::runtime_error("CreateBuffer: size must be > 0");

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth      = sizeBytes;
    desc.Usage           = usage;
    desc.BindFlags       = bindFlags;
    desc.CPUAccessFlags  = cpuAccessFlags;
    desc.MiscFlags           = miscFlags;
    desc.StructureByteStride = structureByteStride;

    D3D11_SUBRESOURCE_DATA srd = {};
    srd.pSysMem = initialData;

    ComPtr<ID3D11Buffer> buffer;
    D3D11CORE_THROW_IF_FAILED(
        core.GetDevice()->CreateBuffer(&desc,
                                        initialData ? &srd : nullptr,
                                        &buffer));
    return D3D11Resource(std::move(buffer));
}

D3D11Resource CreateStructuredBuffer(D3D11Core& core,
                                      UINT elementCount, UINT elementStride,
                                      D3D11_USAGE usage, UINT bindFlags,
                                      const void* initialData) {
    if (elementCount == 0) throw std::runtime_error("CreateStructuredBuffer: elementCount must be > 0");
    if (elementStride == 0) throw std::runtime_error("CreateStructuredBuffer: elementStride must be > 0");

    return CreateBuffer(core,
                        elementCount * elementStride,
                        usage, bindFlags, 0,
                        D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
                        initialData,
                        elementStride);   // StructureByteStride 必須
}

D3D11Resource CreateConstantBuffer(D3D11Core& core, UINT sizeBytes,
                                    const void* initialData) {
    if (sizeBytes == 0) throw std::runtime_error("CreateConstantBuffer: sizeBytes must be > 0");
    // Constant Buffer は 16byte アラインメントが必要。
    const UINT aligned = AlignUp(sizeBytes, 16u);
    return CreateBuffer(core, aligned,
                        D3D11_USAGE_DEFAULT,
                        D3D11_BIND_CONSTANT_BUFFER,
                        0, 0, initialData);
}

// ==========================================================================
// テクスチャ生成
// ==========================================================================

D3D11Resource CreateTexture2D(D3D11Core& core, UINT width, UINT height,
                               DXGI_FORMAT format, UINT bindFlags,
                               D3D11_USAGE usage, UINT miscFlags,
                               UINT16 arraySize, UINT16 mipLevels) {
    if (width == 0 || height == 0) {
        throw std::runtime_error("CreateTexture2D: width and height must be > 0");
    }
    if (arraySize == 0 || mipLevels == 0) {
        throw std::runtime_error("CreateTexture2D: arraySize and mipLevels must be > 0");
    }
    if (FormatUtil::RequiresEvenSize(format) && ((width & 1u) || (height & 1u))) {
        throw std::runtime_error("CreateTexture2D: format requires even width and height");
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width            = width;
    desc.Height           = height;
    desc.MipLevels        = mipLevels;
    desc.ArraySize        = arraySize;
    desc.Format           = format;
    desc.SampleDesc.Count = 1;
    desc.Usage            = usage;
    desc.BindFlags        = bindFlags;
    desc.MiscFlags        = miscFlags;

    // Usage に応じた CPUAccessFlags を自動設定。
    if (usage == D3D11_USAGE_DYNAMIC)
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    else if (usage == D3D11_USAGE_STAGING)
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    ComPtr<ID3D11Texture2D> texture;
    D3D11CORE_THROW_IF_FAILED(
        core.GetDevice()->CreateTexture2D(&desc, nullptr, &texture));
    return D3D11Resource(std::move(texture));
}

D3D11Resource CreateTexture2DFromMemory(D3D11Core& core, const void* data,
                                         UINT width, UINT height,
                                         DXGI_FORMAT format,
                                         UINT srcRowPitch, UINT bindFlags) {
    if (!data) throw std::runtime_error("CreateTexture2DFromMemory: null data");
    ValidateNonPlanarTextureFormat(format, "CreateTexture2DFromMemory");
    if (FormatUtil::RequiresEvenSize(format) && ((width & 1u) || (height & 1u))) {
        throw std::runtime_error("CreateTexture2DFromMemory: format requires even width and height");
    }

    const UINT bpp = BytesPerPixelChecked(format);
    const UINT effectiveSrcPitch = (srcRowPitch != 0) ? srcRowPitch : (width * bpp);

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width            = width;
    desc.Height           = height;
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    desc.Format           = format;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_DEFAULT;
    desc.BindFlags        = bindFlags;

    // D3D11 では初期データ付き CreateTexture2D で Upload + Copy が不要。
    D3D11_SUBRESOURCE_DATA srd = {};
    srd.pSysMem          = data;
    srd.SysMemPitch      = effectiveSrcPitch;
    srd.SysMemSlicePitch = 0;

    ComPtr<ID3D11Texture2D> texture;
    D3D11CORE_THROW_IF_FAILED(
        core.GetDevice()->CreateTexture2D(&desc, &srd, &texture));
    return D3D11Resource(std::move(texture));
}

D3D11Resource CreateTexture2DFromRGBA(D3D11Core& core, const uint8_t* rgba,
                                       UINT width, UINT height, UINT bindFlags) {
    return CreateTexture2DFromMemory(core, rgba, width, height,
                                     DXGI_FORMAT_R8G8B8A8_UNORM, width * 4, bindFlags);
}

std::vector<uint8_t> ExpandRGBtoRGBA(const uint8_t* rgb, UINT width, UINT height,
                                      uint8_t alpha) {
    if (!rgb) throw std::runtime_error("ExpandRGBtoRGBA: null input");
    const size_t pixelCount = static_cast<size_t>(width) * height;
    std::vector<uint8_t> out(pixelCount * 4);
    for (size_t i = 0; i < pixelCount; ++i) {
        out[i * 4 + 0] = rgb[i * 3 + 0];
        out[i * 4 + 1] = rgb[i * 3 + 1];
        out[i * 4 + 2] = rgb[i * 3 + 2];
        out[i * 4 + 3] = alpha;
    }
    return out;
}

D3D11Resource CreateTexture2DFromRGB(D3D11Core& core, const uint8_t* rgb,
                                      UINT width, UINT height, uint8_t alpha,
                                      UINT bindFlags) {
    const std::vector<uint8_t> rgba = ExpandRGBtoRGBA(rgb, width, height, alpha);
    return CreateTexture2DFromMemory(core, rgba.data(), width, height,
                                     DXGI_FORMAT_R8G8B8A8_UNORM, width * 4, bindFlags);
}

D3D11Resource CreateTexture2DFromSubresources(
    D3D11Core& core,
    UINT width,
    UINT height,
    DXGI_FORMAT format,
    const D3D11TextureSubresourceData* subresources,
    UINT subresourceCount,
    UINT bindFlags,
    D3D11_USAGE usage,
    UINT miscFlags,
    UINT16 arraySize,
    UINT16 mipLevels) {

    if (!subresources) throw std::runtime_error("CreateTexture2DFromSubresources: null subresources");
    ValidateNonPlanarTextureFormat(format, "CreateTexture2DFromSubresources");
    if (width == 0 || height == 0) {
        throw std::runtime_error("CreateTexture2DFromSubresources: width and height must be > 0");
    }
    if (arraySize == 0 || mipLevels == 0) {
        throw std::runtime_error("CreateTexture2DFromSubresources: arraySize and mipLevels must be > 0");
    }

    const UINT expectedCount = static_cast<UINT>(arraySize) * static_cast<UINT>(mipLevels);
    if (subresourceCount != expectedCount) {
        throw std::runtime_error("CreateTexture2DFromSubresources: subresourceCount must equal mipLevels * arraySize");
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width            = width;
    desc.Height           = height;
    desc.MipLevels        = mipLevels;
    desc.ArraySize        = arraySize;
    desc.Format           = format;
    desc.SampleDesc.Count = 1;
    desc.Usage            = usage;
    desc.BindFlags        = bindFlags;
    desc.MiscFlags        = miscFlags;

    if (usage == D3D11_USAGE_DYNAMIC) {
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    } else if (usage == D3D11_USAGE_STAGING) {
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    }

    std::vector<D3D11_SUBRESOURCE_DATA> initialData(subresourceCount);
    for (UINT arraySlice = 0; arraySlice < arraySize; ++arraySlice) {
        for (UINT mip = 0; mip < mipLevels; ++mip) {
            const UINT index = D3D11CalcSubresource(mip, arraySlice, mipLevels);
            initialData[index] = MakeSubresourceData(
                subresources[index], width, height, format, mip,
                "CreateTexture2DFromSubresources");
        }
    }

    ComPtr<ID3D11Texture2D> texture;
    D3D11CORE_THROW_IF_FAILED(
        core.GetDevice()->CreateTexture2D(&desc, initialData.data(), &texture));
    return D3D11Resource(std::move(texture));
}

// ==========================================================================
// テクスチャ更新
// ==========================================================================

void UpdateTexture2D(D3D11Core& core, D3D11Resource& dstTexture,
                     const void* data, UINT width, UINT height,
                     DXGI_FORMAT format, UINT srcRowPitch) {
    if (!data) throw std::runtime_error("UpdateTexture2D: null data");
    if (!dstTexture) throw std::runtime_error("UpdateTexture2D: null texture");
    ValidateNonPlanarTextureFormat(format, "UpdateTexture2D");

    ID3D11DeviceContext* ctx = core.GetImmediateContext();

    D3D11_TEXTURE2D_DESC desc = dstTexture.GetTexture2DDesc();
    ValidateSingleSubresourceTexture(desc, "UpdateTexture2D");

    const UINT bpp = BytesPerPixelChecked(format);
    const UINT effectiveSrcPitch = (srcRowPitch != 0) ? srcRowPitch : (width * bpp);

    if (desc.Usage == D3D11_USAGE_DYNAMIC) {
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        D3D11CORE_THROW_IF_FAILED(
            ctx->Map(dstTexture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));

        const auto* src = static_cast<const uint8_t*>(data);
        auto* dst = static_cast<uint8_t*>(mapped.pData);
        const UINT copyWidth = width * bpp;
        for (UINT y = 0; y < height; ++y) {
            std::memcpy(dst + static_cast<size_t>(y) * mapped.RowPitch,
                        src + static_cast<size_t>(y) * effectiveSrcPitch,
                        copyWidth);
        }
        ctx->Unmap(dstTexture.Get(), 0);
    } else if (desc.Usage == D3D11_USAGE_DEFAULT) {
        D3D11_BOX box = {};
        box.left   = 0;
        box.top    = 0;
        box.front  = 0;
        box.right  = width;
        box.bottom = height;
        box.back   = 1;

        ctx->UpdateSubresource(dstTexture.Get(), 0, &box,
                               data, effectiveSrcPitch, 0);
    } else {
        throw std::runtime_error("UpdateTexture2D: only DEFAULT and DYNAMIC textures are supported");
    }
}

void UpdateTextureSubresources(
    D3D11Core& core,
    D3D11Resource& dstTexture,
    const D3D11TextureSubresourceData* subresources,
    UINT firstSubresource,
    UINT subresourceCount) {

    if (!subresources) throw std::runtime_error("UpdateTextureSubresources: null subresources");
    if (!dstTexture) throw std::runtime_error("UpdateTextureSubresources: null texture");

    D3D11_TEXTURE2D_DESC desc = dstTexture.GetTexture2DDesc();
    ValidateNonPlanarTextureFormat(desc.Format, "UpdateTextureSubresources");
    ValidateSubresourceRange(desc, firstSubresource, subresourceCount);

    ID3D11DeviceContext* ctx = core.GetImmediateContext();

    for (UINT i = 0; i < subresourceCount; ++i) {
        const UINT subresource = firstSubresource + i;
        const UINT mip = subresource % desc.MipLevels;
        const D3D11TextureSubresourceData& src = subresources[i];
        const D3D11_SUBRESOURCE_DATA srd =
            MakeSubresourceData(src, desc.Width, desc.Height, desc.Format, mip,
                                "UpdateTextureSubresources");

        if (desc.Usage == D3D11_USAGE_DYNAMIC) {
            D3D11_MAPPED_SUBRESOURCE mapped = {};
            D3D11CORE_THROW_IF_FAILED(
                ctx->Map(dstTexture.Get(), subresource, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
            CopyRowsToMappedSubresource(mapped, src, desc.Width, desc.Height,
                                         desc.Format, mip, "UpdateTextureSubresources");
            ctx->Unmap(dstTexture.Get(), subresource);
        } else if (desc.Usage == D3D11_USAGE_DEFAULT) {
            ctx->UpdateSubresource(dstTexture.Get(), subresource, nullptr,
                                   srd.pSysMem, srd.SysMemPitch, srd.SysMemSlicePitch);
        } else {
            throw std::runtime_error("UpdateTextureSubresources: only DEFAULT and DYNAMIC textures are supported");
        }
    }
}

// ==========================================================================
// View 作成ヘルパ
// ==========================================================================

ComPtr<ID3D11ShaderResourceView> CreateSrv(
    D3D11Core& core,
    ID3D11Resource* resource,
    const D3D11_SHADER_RESOURCE_VIEW_DESC& desc) {
    if (!resource) throw std::runtime_error("CreateSrv: null resource");

    ComPtr<ID3D11ShaderResourceView> view;
    D3D11CORE_THROW_IF_FAILED(
        core.GetDevice()->CreateShaderResourceView(resource, &desc, &view));
    return view;
}

ComPtr<ID3D11UnorderedAccessView> CreateUav(
    D3D11Core& core,
    ID3D11Resource* resource,
    const D3D11_UNORDERED_ACCESS_VIEW_DESC& desc) {
    if (!resource) throw std::runtime_error("CreateUav: null resource");

    ComPtr<ID3D11UnorderedAccessView> view;
    D3D11CORE_THROW_IF_FAILED(
        core.GetDevice()->CreateUnorderedAccessView(resource, &desc, &view));
    return view;
}

ComPtr<ID3D11RenderTargetView> CreateRtv(
    D3D11Core& core,
    ID3D11Resource* resource,
    const D3D11_RENDER_TARGET_VIEW_DESC& desc) {
    if (!resource) throw std::runtime_error("CreateRtv: null resource");

    ComPtr<ID3D11RenderTargetView> view;
    D3D11CORE_THROW_IF_FAILED(
        core.GetDevice()->CreateRenderTargetView(resource, &desc, &view));
    return view;
}

ComPtr<ID3D11DepthStencilView> CreateDsv(
    D3D11Core& core,
    ID3D11Resource* resource,
    const D3D11_DEPTH_STENCIL_VIEW_DESC& desc) {
    if (!resource) throw std::runtime_error("CreateDsv: null resource");

    ComPtr<ID3D11DepthStencilView> view;
    D3D11CORE_THROW_IF_FAILED(
        core.GetDevice()->CreateDepthStencilView(resource, &desc, &view));
    return view;
}

ComPtr<ID3D11ShaderResourceView> CreateTexture2DSrv(
    D3D11Core& core, const D3D11Resource& texture, DXGI_FORMAT format) {
    CheckTexture2DResource(texture, "CreateTexture2DSrv");

    D3D11_TEXTURE2D_DESC texDesc = texture.GetTexture2DDesc();

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format                    = (format == DXGI_FORMAT_UNKNOWN) ? texDesc.Format : format;
    srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels       = texDesc.MipLevels;

    return CreateSrv(core, texture.Get(), srvDesc);
}

ComPtr<ID3D11UnorderedAccessView> CreateTexture2DUav(
    D3D11Core& core, const D3D11Resource& texture, DXGI_FORMAT format,
    UINT mipSlice) {
    CheckTexture2DResource(texture, "CreateTexture2DUav");

    D3D11_TEXTURE2D_DESC texDesc = texture.GetTexture2DDesc();

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format             = (format == DXGI_FORMAT_UNKNOWN) ? texDesc.Format : format;
    uavDesc.ViewDimension      = D3D11_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Texture2D.MipSlice = mipSlice;

    return CreateUav(core, texture.Get(), uavDesc);
}

ComPtr<ID3D11ShaderResourceView> CreateBufferSrv(
    D3D11Core& core, const D3D11Resource& buffer,
    UINT firstElement, UINT numElements, DXGI_FORMAT format) {
    ValidateBufferView(buffer, firstElement, numElements, format, "CreateBufferSrv");

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format              = format;
    srvDesc.ViewDimension       = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = firstElement;
    srvDesc.Buffer.NumElements  = numElements;

    return CreateSrv(core, buffer.Get(), srvDesc);
}

ComPtr<ID3D11UnorderedAccessView> CreateBufferUav(
    D3D11Core& core, const D3D11Resource& buffer,
    UINT firstElement, UINT numElements, DXGI_FORMAT format, UINT flags) {
    ValidateBufferView(buffer, firstElement, numElements, format, "CreateBufferUav");

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format              = format;
    uavDesc.ViewDimension       = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = firstElement;
    uavDesc.Buffer.NumElements  = numElements;
    uavDesc.Buffer.Flags        = flags;

    return CreateUav(core, buffer.Get(), uavDesc);
}

ComPtr<ID3D11RenderTargetView> CreateTexture2DRtv(
    D3D11Core& core, const D3D11Resource& texture, DXGI_FORMAT format,
    UINT mipSlice) {
    CheckTexture2DResource(texture, "CreateTexture2DRtv");

    D3D11_TEXTURE2D_DESC texDesc = texture.GetTexture2DDesc();

    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format             = (format == DXGI_FORMAT_UNKNOWN) ? texDesc.Format : format;
    rtvDesc.ViewDimension      = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = mipSlice;

    return CreateRtv(core, texture.Get(), rtvDesc);
}

ComPtr<ID3D11DepthStencilView> CreateTexture2DDsv(
    D3D11Core& core, const D3D11Resource& texture, DXGI_FORMAT format,
    UINT mipSlice) {
    CheckTexture2DResource(texture, "CreateTexture2DDsv");

    D3D11_TEXTURE2D_DESC texDesc = texture.GetTexture2DDesc();

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format             = (format == DXGI_FORMAT_UNKNOWN) ? texDesc.Format : format;
    dsvDesc.ViewDimension      = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = mipSlice;

    return CreateDsv(core, texture.Get(), dsvDesc);
}

// ==========================================================================
// Sampler ヘルパ
// ==========================================================================

D3D11_SAMPLER_DESC MakeLinearClampSamplerDesc() {
    D3D11_SAMPLER_DESC desc = {};
    desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.MipLODBias = 0.0f;
    desc.MaxAnisotropy = 1;
    desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    desc.BorderColor[0] = 0.0f;
    desc.BorderColor[1] = 0.0f;
    desc.BorderColor[2] = 0.0f;
    desc.BorderColor[3] = 0.0f;
    desc.MinLOD = 0.0f;
    desc.MaxLOD = D3D11_FLOAT32_MAX;
    return desc;
}

D3D11_SAMPLER_DESC MakePointClampSamplerDesc() {
    D3D11_SAMPLER_DESC desc = MakeLinearClampSamplerDesc();
    desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    return desc;
}

ComPtr<ID3D11SamplerState> CreateSampler(
    D3D11Core& core,
    const D3D11_SAMPLER_DESC& desc) {
    ComPtr<ID3D11SamplerState> sampler;
    D3D11CORE_THROW_IF_FAILED(
        core.GetDevice()->CreateSamplerState(&desc, &sampler));
    return sampler;
}

// ==========================================================================
// 共有リソース
// ==========================================================================

D3D11Resource CreateSharedTexture2D(D3D11Core& core,
                                     UINT width, UINT height, DXGI_FORMAT format,
                                     UINT bindFlags) {
    return CreateSharedTexture2D(core, width, height, format, bindFlags,
                                 D3D11SharedTextureSyncMode::SharedFence);
}

} // namespace D3D11CoreLib
