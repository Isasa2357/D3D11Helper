//
// test_texture_transfer.cpp
//

#include "TestUtil.hpp"

#include <D3D11Helper/D3D11Gpu/D3D11TextureTransfer.hpp>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

using namespace D3D11CoreLib;

namespace {

std::vector<uint8_t> MakeRgbaPattern(UINT width, UINT height, UINT salt = 0) {
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);

    for (UINT y = 0; y < height; ++y) {
        for (UINT x = 0; x < width; ++x) {
            const size_t i = (static_cast<size_t>(y) * width + x) * 4;
            pixels[i + 0] = static_cast<uint8_t>((x * 7 + y * 3 + salt) & 0xFF);
            pixels[i + 1] = static_cast<uint8_t>((x * 5 + y * 11 + salt * 3) & 0xFF);
            pixels[i + 2] = static_cast<uint8_t>((x * 13 + y * 17 + salt * 5) & 0xFF);
            pixels[i + 3] = static_cast<uint8_t>(255 - ((x + y + salt) & 0x3F));
        }
    }

    return pixels;
}

D3D11CpuImage MakeRgbaCpuImage(UINT width, UINT height, UINT rowPitch = 0, UINT salt = 0) {
    auto packed = MakeRgbaPattern(width, height, salt);
    auto image = CreateCpuImage(width, height, DXGI_FORMAT_R8G8B8A8_UNORM, rowPitch);

    const UINT rowBytes = width * 4;
    UnpackRows(image.pixels.data() + static_cast<size_t>(image.planes[0].offsetBytes),
               image.planes[0].rowPitch,
               packed.data(),
               rowBytes,
               height);
    return image;
}

std::vector<uint8_t> PackCpuImagePixels(const D3D11CpuImage& image) {
    ValidateCpuImage(image, "PackCpuImagePixels");
    const UINT rowBytes = GetPackedRowPitch(image.width, image.format);
    return PackRows(image.pixels.data() + static_cast<size_t>(image.planes[0].offsetBytes),
                    image.planes[0].rowPitch,
                    rowBytes,
                    image.height);
}

void ExpectPixelsEqual(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    if (a.size() != b.size()) {
        throw std::runtime_error("pixel size mismatch");
    }

    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) {
            throw std::runtime_error("pixel mismatch at byte " + std::to_string(i));
        }
    }
}

std::vector<uint8_t> ExtractRgbaRegion(
    const std::vector<uint8_t>& src,
    UINT srcWidth,
    UINT left,
    UINT top,
    UINT width,
    UINT height) {

    std::vector<uint8_t> out(static_cast<size_t>(width) * height * 4);

    for (UINT y = 0; y < height; ++y) {
        const size_t srcOffset = (static_cast<size_t>(top + y) * srcWidth + left) * 4;
        const size_t dstOffset = static_cast<size_t>(y) * width * 4;
        std::copy(src.begin() + static_cast<std::ptrdiff_t>(srcOffset),
                  src.begin() + static_cast<std::ptrdiff_t>(srcOffset + static_cast<size_t>(width) * 4),
                  out.begin() + static_cast<std::ptrdiff_t>(dstOffset));
    }

    return out;
}

} // namespace

int main() {
    auto core = TestUtil::MakeCore();

    TEST_RUN("ReadbackTexture2DToCpuImage RGBA8 full texture", {
        constexpr UINT width = 32;
        constexpr UINT height = 16;

        auto srcPixels = MakeRgbaPattern(width, height);
        auto texture = CreateTexture2DFromRGBA(*core, srcPixels.data(), width, height);

        D3D11CpuImage image = ReadbackTexture2DToCpuImage(*core, texture);

        if (image.width != width) throw std::runtime_error("width mismatch");
        if (image.height != height) throw std::runtime_error("height mismatch");
        if (image.format != DXGI_FORMAT_R8G8B8A8_UNORM) throw std::runtime_error("format mismatch");
        if (image.PlaneCount() != 1) throw std::runtime_error("plane count mismatch");
        if (image.planes[0].rowPitch != width * 4) throw std::runtime_error("row pitch mismatch");

        ExpectPixelsEqual(image.pixels, srcPixels);
    });

    TEST_RUN("ReadbackTexture2DRegionToCpuImage RGBA8 sub-rect", {
        constexpr UINT width = 16;
        constexpr UINT height = 12;

        auto srcPixels = MakeRgbaPattern(width, height);
        auto texture = CreateTexture2DFromRGBA(*core, srcPixels.data(), width, height);

        D3D11_BOX box{};
        box.left = 3;
        box.top = 2;
        box.front = 0;
        box.right = 11;
        box.bottom = 8;
        box.back = 1;

        D3D11CpuImage image = ReadbackTexture2DRegionToCpuImage(*core, texture, box);

        const UINT regionWidth = box.right - box.left;
        const UINT regionHeight = box.bottom - box.top;

        if (image.width != regionWidth) throw std::runtime_error("region width mismatch");
        if (image.height != regionHeight) throw std::runtime_error("region height mismatch");
        if (image.format != DXGI_FORMAT_R8G8B8A8_UNORM) throw std::runtime_error("region format mismatch");
        if (image.planes[0].rowPitch != regionWidth * 4) throw std::runtime_error("region row pitch mismatch");

        auto expected = ExtractRgbaRegion(srcPixels, width, box.left, box.top, regionWidth, regionHeight);
        ExpectPixelsEqual(image.pixels, expected);
    });

    TEST_RUN("CreateTexture2DFromCpuImage RGBA8 packed rows", {
        constexpr UINT width = 24;
        constexpr UINT height = 10;

        auto image = MakeRgbaCpuImage(width, height, 0, 1);
        auto texture = CreateTexture2DFromCpuImage(*core, image);
        auto readback = ReadbackTexture2DToCpuImage(*core, texture);

        ExpectPixelsEqual(readback.pixels, PackCpuImagePixels(image));
    });

    TEST_RUN("CreateTexture2DFromCpuImage RGBA8 padded rows", {
        constexpr UINT width = 13;
        constexpr UINT height = 9;
        constexpr UINT rowPitch = width * 4 + 16;

        auto image = MakeRgbaCpuImage(width, height, rowPitch, 2);
        auto texture = CreateTexture2DFromCpuImage(*core, image);
        auto readback = ReadbackTexture2DToCpuImage(*core, texture);

        if (readback.planes[0].rowPitch != width * 4) {
            throw std::runtime_error("readback should be tightly packed");
        }
        ExpectPixelsEqual(readback.pixels, PackCpuImagePixels(image));
    });

    TEST_RUN("UpdateTexture2DFromCpuImage RGBA8 DEFAULT", {
        constexpr UINT width = 20;
        constexpr UINT height = 7;

        auto initial = MakeRgbaCpuImage(width, height, 0, 3);
        auto updated = MakeRgbaCpuImage(width, height, width * 4 + 8, 4);

        auto texture = CreateTexture2DFromCpuImage(*core, initial);
        UpdateTexture2DFromCpuImage(*core, texture, updated);
        auto readback = ReadbackTexture2DToCpuImage(*core, texture);

        ExpectPixelsEqual(readback.pixels, PackCpuImagePixels(updated));
    });

    TEST_RUN("UpdateTexture2DFromCpuImage RGBA8 DYNAMIC", {
        constexpr UINT width = 18;
        constexpr UINT height = 11;

        auto initial = MakeRgbaCpuImage(width, height, 0, 5);
        auto updated = MakeRgbaCpuImage(width, height, width * 4 + 12, 6);

        auto texture = CreateTexture2DFromCpuImage(
            *core,
            initial,
            D3D11_BIND_SHADER_RESOURCE,
            D3D11_USAGE_DYNAMIC);
        UpdateTexture2DFromCpuImage(*core, texture, updated);
        auto readback = ReadbackTexture2DToCpuImage(*core, texture);

        ExpectPixelsEqual(readback.pixels, PackCpuImagePixels(updated));
    });

    TEST_RUN("ReadbackTexture2DToCpuImage rejects null resource", {
        bool threw = false;
        try {
            D3D11Resource empty;
            (void)ReadbackTexture2DToCpuImage(*core, empty);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        if (!threw) throw std::runtime_error("expected invalid_argument");
    });

    TEST_RUN("ReadbackTexture2DRegionToCpuImage rejects invalid box", {
        constexpr UINT width = 8;
        constexpr UINT height = 8;

        auto srcPixels = MakeRgbaPattern(width, height);
        auto texture = CreateTexture2DFromRGBA(*core, srcPixels.data(), width, height);

        D3D11_BOX box{};
        box.left = 4;
        box.top = 1;
        box.front = 0;
        box.right = 4;
        box.bottom = 3;
        box.back = 1;

        bool threw = false;
        try {
            (void)ReadbackTexture2DRegionToCpuImage(*core, texture, box);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        if (!threw) throw std::runtime_error("expected invalid_argument");
    });

    TEST_RUN("UpdateTexture2DFromCpuImage rejects size mismatch", {
        auto initial = MakeRgbaCpuImage(8, 8, 0, 7);
        auto wrongSize = MakeRgbaCpuImage(4, 8, 0, 8);
        auto texture = CreateTexture2DFromCpuImage(*core, initial);

        bool threw = false;
        try {
            UpdateTexture2DFromCpuImage(*core, texture, wrongSize);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        if (!threw) throw std::runtime_error("expected invalid_argument");
    });

    TEST_RUN("UpdateTexture2DFromCpuImage rejects format mismatch", {
        auto initial = MakeRgbaCpuImage(8, 8, 0, 9);
        auto wrongFormat = CreateCpuImage(8, 8, DXGI_FORMAT_R8_UNORM);
        auto texture = CreateTexture2DFromCpuImage(*core, initial);

        bool threw = false;
        try {
            UpdateTexture2DFromCpuImage(*core, texture, wrongFormat);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        if (!threw) throw std::runtime_error("expected invalid_argument");
    });

    return TestUtil::Result("TextureTransfer");
}
