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

std::vector<uint8_t> MakeRgbaPattern(UINT width, UINT height) {
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);

    for (UINT y = 0; y < height; ++y) {
        for (UINT x = 0; x < width; ++x) {
            const size_t i = (static_cast<size_t>(y) * width + x) * 4;
            pixels[i + 0] = static_cast<uint8_t>((x * 7 + y * 3) & 0xFF);
            pixels[i + 1] = static_cast<uint8_t>((x * 5 + y * 11) & 0xFF);
            pixels[i + 2] = static_cast<uint8_t>((x * 13 + y * 17) & 0xFF);
            pixels[i + 3] = static_cast<uint8_t>(255 - ((x + y) & 0x3F));
        }
    }

    return pixels;
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

    return TestUtil::Result("TextureTransfer");
}
