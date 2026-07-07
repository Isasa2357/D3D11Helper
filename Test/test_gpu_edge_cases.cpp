//
// test_gpu_edge_cases.cpp - D3D11Gpu transfer/copy/view edge-case tests.
//
#include "TestUtil.hpp"

#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace D3D11CoreLib;

namespace {

void ExpectThrows(const char* label, const std::function<void()>& fn) {
    bool threw = false;
    try {
        fn();
    } catch (...) {
        threw = true;
    }
    if (!threw) {
        throw std::runtime_error(std::string(label) + " did not throw");
    }
}

void RequireBytesEqual(const std::vector<uint8_t>& actual, const std::vector<uint8_t>& expected, const char* label) {
    if (actual.size() != expected.size()) {
        throw std::runtime_error(std::string(label) + ": size mismatch");
    }
    for (size_t i = 0; i < expected.size(); ++i) {
        if (actual[i] != expected[i]) {
            throw std::runtime_error(
                std::string(label) + ": mismatch at byte " + std::to_string(i) +
                " actual=" + std::to_string(actual[i]) +
                " expected=" + std::to_string(expected[i]));
        }
    }
}

std::vector<uint8_t> PackCpuImage(const D3D11CpuImage& image) {
    ValidateCpuImage(image, "PackCpuImage");
    const UINT rowBytes = GetPackedRowPitch(image.width, image.format);
    const auto& plane = image.planes[0];
    return PackRows(image.pixels.data() + static_cast<size_t>(plane.offsetBytes),
                    plane.rowPitch,
                    rowBytes,
                    plane.height);
}

D3D11CpuImage MakeRgbaImageWithPitch(UINT width, UINT height, UINT rowPitch, uint8_t baseValue) {
    D3D11CpuImage image = CreateCpuImage(width, height, DXGI_FORMAT_R8G8B8A8_UNORM, rowPitch);
    const UINT rowBytes = GetPackedRowPitch(width, image.format);
    for (UINT y = 0; y < height; ++y) {
        auto* row = image.pixels.data() + static_cast<size_t>(image.planes[0].offsetBytes) +
                    static_cast<size_t>(y) * image.planes[0].rowPitch;
        for (UINT x = 0; x < width; ++x) {
            const size_t i = static_cast<size_t>(x) * 4u;
            row[i + 0] = static_cast<uint8_t>(baseValue + x + y * 17u);
            row[i + 1] = static_cast<uint8_t>(baseValue + 40u + x + y * 13u);
            row[i + 2] = static_cast<uint8_t>(baseValue + 80u + x + y * 7u);
            row[i + 3] = 255;
        }
        for (UINT i = rowBytes; i < image.planes[0].rowPitch; ++i) {
            row[i] = 0xCD;
        }
    }
    return image;
}

std::vector<uint8_t> ReadbackBufferBytes(D3D11Core& core, ID3D11Buffer* buffer, UINT sizeBytes) {
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = sizeBytes;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;

    ComPtr<ID3D11Buffer> staging;
    D3D11CORE_THROW_IF_FAILED(core.GetDevice()->CreateBuffer(&desc, nullptr, &staging));
    core.GetImmediateContext()->CopyResource(staging.Get(), buffer);
    core.Flush();

    D3D11_MAPPED_SUBRESOURCE mapped{};
    D3D11CORE_THROW_IF_FAILED(core.GetImmediateContext()->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));
    std::vector<uint8_t> out(sizeBytes);
    std::memcpy(out.data(), mapped.pData, sizeBytes);
    core.GetImmediateContext()->Unmap(staging.Get(), 0);
    return out;
}

} // namespace

int main() {
    auto core = TestUtil::MakeCore();
    ID3D11Device* device = core->GetDevice();
    ID3D11DeviceContext* context = core->GetImmediateContext();

    TEST_RUN("CpuImage row pitch and row helper validation", {
        if (GetPackedRowPitch(3, DXGI_FORMAT_R8G8B8A8_UNORM) != 12) {
            throw std::runtime_error("unexpected RGBA8 packed row pitch");
        }
        if (GetRequiredCpuImageSize(3, 2, DXGI_FORMAT_R8G8B8A8_UNORM, 16) != 32) {
            throw std::runtime_error("unexpected padded required image size");
        }

        ExpectThrows("zero width row pitch", [&] {
            (void)GetPackedRowPitch(0, DXGI_FORMAT_R8G8B8A8_UNORM);
        });
        ExpectThrows("unsupported cpu image format", [&] {
            (void)CreateCpuImage(4, 4, DXGI_FORMAT_UNKNOWN);
        });
        ExpectThrows("row pitch too small", [&] {
            (void)CreateCpuImage(4, 4, DXGI_FORMAT_R8G8B8A8_UNORM, 15);
        });
        ExpectThrows("copy rows bad destination pitch", [&] {
            uint8_t src[16]{};
            uint8_t dst[16]{};
            CopyRows(dst, 3, src, 4, 4, 1);
        });

        const uint8_t srcRows[10] = { 1, 2, 3, 4, 0xEE, 5, 6, 7, 8, 0xEE };
        const auto packed = PackRows(srcRows, 5, 4, 2);
        RequireBytesEqual(packed, { 1, 2, 3, 4, 5, 6, 7, 8 }, "PackRows");

        uint8_t unpacked[10]{};
        UnpackRows(unpacked, 5, packed.data(), 4, 2);
        if (unpacked[0] != 1 || unpacked[3] != 4 || unpacked[5] != 5 || unpacked[8] != 8) {
            throw std::runtime_error("UnpackRows did not write expected row bytes");
        }
    });

    TEST_RUN("TextureTransfer padded upload readback and update", {
        auto image = MakeRgbaImageWithPitch(3, 2, 16, 10);
        const auto expected = PackCpuImage(image);

        auto texture = CreateTexture2DFromCpuImage(*core, image, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
        auto readback = ReadbackTexture2DToCpuImage(*core, texture);
        RequireBytesEqual(PackCpuImage(readback), expected, "padded upload readback");

        auto updatedImage = MakeRgbaImageWithPitch(3, 2, 20, 90);
        const auto updatedExpected = PackCpuImage(updatedImage);
        UpdateTexture2DFromCpuImage(*core, texture, updatedImage);
        auto updatedReadback = ReadbackTexture2DToCpuImage(*core, texture);
        RequireBytesEqual(PackCpuImage(updatedReadback), updatedExpected, "padded update readback");
    });

    TEST_RUN("TextureTransfer region readback and invalid boxes", {
        auto image = MakeRgbaImageWithPitch(4, 3, 20, 30);
        auto texture = CreateTexture2DFromCpuImage(*core, image, D3D11_BIND_SHADER_RESOURCE);

        D3D11_BOX box{};
        box.left = 1;
        box.top = 1;
        box.front = 0;
        box.right = 3;
        box.bottom = 3;
        box.back = 1;
        auto region = ReadbackTexture2DRegionToCpuImage(*core, texture, box);

        std::vector<uint8_t> expected;
        const UINT srcRowPitch = image.planes[0].rowPitch;
        const auto* srcBase = image.pixels.data();
        for (UINT y = 1; y < 3; ++y) {
            const auto* src = srcBase + static_cast<size_t>(y) * srcRowPitch + 1u * 4u;
            expected.insert(expected.end(), src, src + 2u * 4u);
        }
        RequireBytesEqual(PackCpuImage(region), expected, "region readback");

        D3D11_BOX empty = box;
        empty.right = empty.left;
        ExpectThrows("empty readback region", [&] {
            (void)ReadbackTexture2DRegionToCpuImage(*core, texture, empty);
        });

        D3D11_BOX badDepth = box;
        badDepth.back = 2;
        ExpectThrows("bad Texture2D box depth", [&] {
            (void)ReadbackTexture2DRegionToCpuImage(*core, texture, badDepth);
        });

        D3D11_BOX outside = box;
        outside.right = 5;
        ExpectThrows("outside readback region", [&] {
            (void)ReadbackTexture2DRegionToCpuImage(*core, texture, outside);
        });
    });

    TEST_RUN("Copy helpers region validation and buffer copy", {
        if (CalcD3D11Subresource(2, 3, 4) != 14) {
            throw std::runtime_error("CalcD3D11Subresource returned unexpected value");
        }
        ExpectThrows("CalcD3D11Subresource zero mip levels", [&] {
            (void)CalcD3D11Subresource(0, 0, 0);
        });
        ExpectThrows("CalcD3D11Subresource mip out of range", [&] {
            (void)CalcD3D11Subresource(4, 0, 4);
        });

        auto srcImage = MakeRgbaImageWithPitch(4, 4, 16, 5);
        auto dstImage = CreateCpuImage(4, 4, DXGI_FORMAT_R8G8B8A8_UNORM);
        auto src = CreateTexture2DFromCpuImage(*core, srcImage, D3D11_BIND_SHADER_RESOURCE);
        auto dst = CreateTexture2DFromCpuImage(*core, dstImage, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

        D3D11Texture2DRegion region{};
        region.srcX = 1;
        region.srcY = 1;
        region.dstX = 0;
        region.dstY = 0;
        region.width = 2;
        region.height = 2;
        CopyTexture2DRegion(context, dst.AsTexture2D(), src.AsTexture2D(), region);

        auto copied = ReadbackTexture2DToCpuImage(*core, dst);
        const auto copiedPacked = PackCpuImage(copied);
        for (UINT y = 0; y < 2; ++y) {
            const auto* srcRow = srcImage.pixels.data() + static_cast<size_t>(1 + y) * srcImage.planes[0].rowPitch + 1u * 4u;
            const auto* dstRow = copiedPacked.data() + static_cast<size_t>(y) * 4u * 4u;
            if (std::memcmp(srcRow, dstRow, 2u * 4u) != 0) {
                throw std::runtime_error("CopyTexture2DRegion copied unexpected bytes");
            }
        }

        region.width = 0;
        ExpectThrows("zero-width texture copy region", [&] {
            CopyTexture2DRegion(context, dst.AsTexture2D(), src.AsTexture2D(), region);
        });

        std::array<uint32_t, 8> srcValues = { 10, 20, 30, 40, 50, 60, 70, 80 };
        std::array<uint32_t, 8> dstValues = { 0, 0, 0, 0, 0, 0, 0, 0 };
        auto srcBuffer = CreateBuffer(*core, static_cast<UINT>(srcValues.size() * sizeof(uint32_t)),
                                      D3D11_USAGE_DEFAULT, 0, 0, 0, srcValues.data());
        auto dstBuffer = CreateBuffer(*core, static_cast<UINT>(dstValues.size() * sizeof(uint32_t)),
                                      D3D11_USAGE_DEFAULT, 0, 0, 0, dstValues.data());

        D3D11BufferCopyRegion bufferRegion{};
        bufferRegion.srcOffset = 2u * sizeof(uint32_t);
        bufferRegion.dstOffset = 1u * sizeof(uint32_t);
        bufferRegion.sizeBytes = 3u * sizeof(uint32_t);
        CopyBufferRegion(context, dstBuffer.AsBuffer(), srcBuffer.AsBuffer(), bufferRegion);

        const auto bytes = ReadbackBufferBytes(*core, dstBuffer.AsBuffer(), static_cast<UINT>(dstValues.size() * sizeof(uint32_t)));
        std::array<uint32_t, 8> readback{};
        std::memcpy(readback.data(), bytes.data(), bytes.size());
        if (readback[0] != 0 || readback[1] != 30 || readback[2] != 40 || readback[3] != 50 || readback[4] != 0) {
            throw std::runtime_error("CopyBufferRegion produced unexpected contents");
        }

        bufferRegion.sizeBytes = 0;
        ExpectThrows("zero-byte buffer copy", [&] {
            CopyBufferRegion(context, dstBuffer.AsBuffer(), srcBuffer.AsBuffer(), bufferRegion);
        });
    });

    TEST_RUN("Depth view helper mappings and validation", {
        if (GetTypelessDepthTextureFormat(DXGI_FORMAT_D24_UNORM_S8_UINT) != DXGI_FORMAT_R24G8_TYPELESS) {
            throw std::runtime_error("unexpected typeless depth format mapping");
        }
        if (GetDepthStencilViewFormat(DXGI_FORMAT_R24G8_TYPELESS) != DXGI_FORMAT_D24_UNORM_S8_UINT) {
            throw std::runtime_error("unexpected DSV depth format mapping");
        }
        if (GetDepthShaderResourceViewFormat(DXGI_FORMAT_R24G8_TYPELESS) != DXGI_FORMAT_R24_UNORM_X8_TYPELESS) {
            throw std::runtime_error("unexpected depth SRV format mapping");
        }
        if (GetStencilShaderResourceViewFormat(DXGI_FORMAT_R24G8_TYPELESS) != DXGI_FORMAT_X24_TYPELESS_G8_UINT) {
            throw std::runtime_error("unexpected stencil SRV format mapping");
        }

        auto depth = CreateTexture2D(*core,
                                     8,
                                     8,
                                     DXGI_FORMAT_R24G8_TYPELESS,
                                     D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE);
        auto dsv = CreateDepthTexture2DDsv(device, depth.AsTexture2D());
        auto srv = CreateDepthTexture2DSrv(device, depth.AsTexture2D());
        if (!dsv || !srv) {
            throw std::runtime_error("depth DSV/SRV creation failed");
        }

        ExpectThrows("depth DSV null device", [&] {
            (void)CreateDepthTexture2DDsv(nullptr, depth.AsTexture2D());
        });
        ExpectThrows("depth SRV null texture", [&] {
            (void)CreateDepthTexture2DSrv(device, nullptr);
        });
    });

    return TestUtil::Result("GpuEdgeCases");
}
