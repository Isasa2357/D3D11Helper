//
// test_texture_transfer.cpp - D3D11CpuImage <-> Texture2D transfer tests
//
#include "TestUtil.hpp"
#include <D3D11Helper/D3D11Gpu/D3D11TextureTransfer.hpp>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

using namespace D3D11CoreLib;

namespace {

D3D11CpuImage MakeRgbaPattern(UINT width, UINT height, UINT rowPitch = 0, uint8_t seed = 0) {
    auto image = CreateCpuImage(width, height, DXGI_FORMAT_R8G8B8A8_UNORM, rowPitch);
    const UINT rowBytes = GetPackedRowPitch(width, image.format);

    for (UINT y = 0; y < height; ++y) {
        uint8_t* row = image.pixels.data() + image.planes[0].offsetBytes + y * image.planes[0].rowPitch;
        for (UINT x = 0; x < width; ++x) {
            row[x * 4 + 0] = static_cast<uint8_t>(seed + x);
            row[x * 4 + 1] = static_cast<uint8_t>(seed + y * 3);
            row[x * 4 + 2] = static_cast<uint8_t>(seed + x + y);
            row[x * 4 + 3] = 255;
        }
        for (UINT i = rowBytes; i < image.planes[0].rowPitch; ++i) {
            row[i] = 0xCD;
        }
    }

    return image;
}

bool EqualPackedPixels(const D3D11CpuImage& a, const D3D11CpuImage& b) {
    if (a.width != b.width || a.height != b.height || a.format != b.format) return false;
    const UINT rowBytes = GetPackedRowPitch(a.width, a.format);

    for (UINT y = 0; y < a.height; ++y) {
        const uint8_t* ar = a.pixels.data() + a.planes[0].offsetBytes + y * a.planes[0].rowPitch;
        const uint8_t* br = b.pixels.data() + b.planes[0].offsetBytes + y * b.planes[0].rowPitch;
        if (!std::equal(ar, ar + rowBytes, br)) return false;
    }
    return true;
}

void ExpectThrows(const char* label, const std::function<void()>& fn) {
    bool threw = false;
    try {
        fn();
    } catch (...) {
        threw = true;
    }
    if (!threw) throw std::runtime_error(std::string(label) + " did not throw");
}

} // namespace

int main() {
    auto core = TestUtil::MakeCore();

    TEST_RUN("CreateTexture2DFromCpuImage + Readback roundtrip RGBA8", {
        auto src = MakeRgbaPattern(32, 16, 0, 7);
        auto tex = CreateTexture2DFromCpuImage(*core, src,
            D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
            D3D11_USAGE_DEFAULT);
        auto rb = ReadbackTexture2DToCpuImage(*core, tex);
        if (!EqualPackedPixels(src, rb)) throw std::runtime_error("roundtrip mismatch");
    });

    TEST_RUN("CreateTexture2DFromCpuImage accepts padded row pitch", {
        const UINT rowBytes = GetPackedRowPitch(17, DXGI_FORMAT_R8G8B8A8_UNORM);
        auto src = MakeRgbaPattern(17, 9, rowBytes + 32, 11);
        auto tex = CreateTexture2DFromCpuImage(*core, src,
            D3D11_BIND_SHADER_RESOURCE,
            D3D11_USAGE_DEFAULT);
        auto rb = ReadbackTexture2DToCpuImage(*core, tex);
        if (!EqualPackedPixels(src, rb)) throw std::runtime_error("padded roundtrip mismatch");
    });

    TEST_RUN("UpdateTexture2DFromCpuImage DEFAULT", {
        auto a = MakeRgbaPattern(16, 16, 0, 1);
        auto b = MakeRgbaPattern(16, 16, 0, 99);
        auto tex = CreateTexture2DFromCpuImage(*core, a,
            D3D11_BIND_SHADER_RESOURCE,
            D3D11_USAGE_DEFAULT);
        UpdateTexture2DFromCpuImage(*core, tex, b);
        auto rb = ReadbackTexture2DToCpuImage(*core, tex);
        if (!EqualPackedPixels(b, rb)) throw std::runtime_error("updated DEFAULT mismatch");
    });

    TEST_RUN("UpdateTexture2DFromCpuImage DYNAMIC no throw", {
        auto a = MakeRgbaPattern(8, 8, 0, 2);
        auto b = MakeRgbaPattern(8, 8, 0, 3);
        auto tex = CreateTexture2DFromCpuImage(*core, a,
            D3D11_BIND_SHADER_RESOURCE,
            D3D11_USAGE_DYNAMIC);
        UpdateTexture2DFromCpuImage(*core, tex, b);
    });

    TEST_RUN("ReadbackTexture2DRegionToCpuImage", {
        auto src = MakeRgbaPattern(8, 8, 0, 21);
        auto tex = CreateTexture2DFromCpuImage(*core, src,
            D3D11_BIND_SHADER_RESOURCE,
            D3D11_USAGE_DEFAULT);

        D3D11_BOX box{};
        box.left = 2;
        box.top = 3;
        box.front = 0;
        box.right = 6;
        box.bottom = 7;
        box.back = 1;

        auto region = ReadbackTexture2DRegionToCpuImage(*core, tex, box);
        if (region.width != 4 || region.height != 4) throw std::runtime_error("bad region size");

        const UINT rowBytes = GetPackedRowPitch(region.width, region.format);
        for (UINT y = 0; y < region.height; ++y) {
            const uint8_t* regionRow = region.pixels.data() + y * region.planes[0].rowPitch;
            const uint8_t* srcRow = src.pixels.data() + (box.top + y) * src.planes[0].rowPitch + box.left * 4;
            if (!std::equal(regionRow, regionRow + rowBytes, srcRow)) {
                throw std::runtime_error("region pixel mismatch");
            }
        }
    });

    TEST_RUN("UpdateTexture2DFromCpuImage rejects mismatch", {
        auto a = MakeRgbaPattern(8, 8, 0, 1);
        auto b = MakeRgbaPattern(9, 8, 0, 1);
        auto tex = CreateTexture2DFromCpuImage(*core, a,
            D3D11_BIND_SHADER_RESOURCE,
            D3D11_USAGE_DEFAULT);
        ExpectThrows("size mismatch", [&] { UpdateTexture2DFromCpuImage(*core, tex, b); });
    });

    TEST_RUN("CreateTexture2DFromCpuImage rejects staging usage", {
        auto a = MakeRgbaPattern(8, 8, 0, 1);
        ExpectThrows("staging usage", [&] {
            (void)CreateTexture2DFromCpuImage(*core, a, 0, D3D11_USAGE_STAGING);
        });
    });

    return TestUtil::Result("TextureTransfer");
}
