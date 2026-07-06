#include "TestCommon.hpp"

#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
#include <D3D11Helper/D3D11Processing/D3D11Processing.hpp>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <utility>
#include <vector>

using namespace D3D11CoreLib;
using namespace D3D11CoreLib::Processing;

namespace {

std::filesystem::path ProcessingShaderDir() {
    const auto runtimeDir = std::filesystem::current_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(runtimeDir / "PyramidDownsample2xRgba.hlsl")) {
        return runtimeDir;
    }

#ifdef D3D11HELPER_TEST_SOURCE_DIR
    const auto sourceDir = std::filesystem::u8path(D3D11HELPER_TEST_SOURCE_DIR).parent_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(sourceDir / "PyramidDownsample2xRgba.hlsl")) {
        return sourceDir;
    }
#endif

    return runtimeDir;
}

struct Fixture {
    std::shared_ptr<D3D11Core> core;
    D3D11ProcessingContext processing;

    explicit Fixture(std::shared_ptr<D3D11Core> c)
        : core(std::move(c)) {
        processing.Initialize(*core, ProcessingShaderDir());
    }
};

std::vector<uint8_t> ReadbackRgba8(D3D11Core& core, D3D11Resource& tex) {
    const auto desc = tex.GetTexture2DDesc();

    D3D11_TEXTURE2D_DESC stagingDesc = desc;
    stagingDesc.BindFlags = 0;
    stagingDesc.MiscFlags = 0;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    ComPtr<ID3D11Texture2D> staging;
    D3D11CORE_THROW_IF_FAILED(core.GetDevice()->CreateTexture2D(&stagingDesc, nullptr, &staging));

    core.GetImmediateContext()->CopyResource(staging.Get(), tex.Get());

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    D3D11CORE_THROW_IF_FAILED(core.GetImmediateContext()->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));

    std::vector<uint8_t> out(static_cast<size_t>(desc.Width) * desc.Height * 4u);
    for (UINT y = 0; y < desc.Height; ++y) {
        std::memcpy(
            out.data() + static_cast<size_t>(y) * desc.Width * 4u,
            static_cast<const uint8_t*>(mapped.pData) + static_cast<size_t>(y) * mapped.RowPitch,
            static_cast<size_t>(desc.Width) * 4u);
    }

    core.GetImmediateContext()->Unmap(staging.Get(), 0);
    return out;
}

void RequireByteNear(uint8_t actual, uint8_t expected, uint8_t tolerance, const char* label, size_t index) {
    const int diff = actual > expected ? actual - expected : expected - actual;
    if (diff > tolerance) {
        std::ostringstream os;
        os << label << ": byte mismatch at index " << index
           << " actual=" << int(actual)
           << " expected=" << int(expected)
           << " tolerance=" << int(tolerance);
        ::Test::Fail(__FILE__, __LINE__, os.str());
    }
}

std::vector<uint8_t> MakeQuadrant4x4() {
    std::vector<uint8_t> pixels(4u * 4u * 4u, 0);
    for (UINT y = 0; y < 4; ++y) {
        for (UINT x = 0; x < 4; ++x) {
            const size_t i = static_cast<size_t>(y * 4u + x) * 4u;
            const uint8_t v = (y < 2)
                ? ((x < 2) ? 10 : 40)
                : ((x < 2) ? 70 : 100);
            pixels[i + 0] = v;
            pixels[i + 1] = static_cast<uint8_t>(v + 1);
            pixels[i + 2] = static_cast<uint8_t>(v + 2);
            pixels[i + 3] = 255;
        }
    }
    return pixels;
}

std::vector<uint8_t> MakePointUpsampleExpected2x2To4x4(const std::vector<uint8_t>& src) {
    std::vector<uint8_t> expected(4u * 4u * 4u, 0);
    for (UINT y = 0; y < 4; ++y) {
        for (UINT x = 0; x < 4; ++x) {
            const UINT sx = x / 2u;
            const UINT sy = y / 2u;
            std::memcpy(
                expected.data() + static_cast<size_t>(y * 4u + x) * 4u,
                src.data() + static_cast<size_t>(sy * 2u + sx) * 4u,
                4u);
        }
    }
    return expected;
}

} // namespace

TEST(ProcessingPyramid, ShaderCompile) {
    REQUIRE_CORE(core);

    Fixture fx(core);

    D3D11ProcessingShaderCache cache;
    cache.Initialize(fx.processing);

    CHECK(!cache.GetComputeShader("PyramidDownsample2xRgba.hlsl").Empty());
    CHECK(!cache.GetComputeShader("PyramidUpsample2xRgba.hlsl").Empty());
}

TEST(ProcessingPyramid, Downsample2xReadback) {
    REQUIRE_CORE(core);

    Fixture fx(core);

    if (!fx.processing.SupportsRgba8Uav()) {
        std::cout << "R8G8B8A8 UAV not supported; skipping\n";
        return;
    }

    const auto srcPixels = MakeQuadrant4x4();
    auto src = CreateTexture2DFromRGBA(*core, srcPixels.data(), 4, 4, D3D11_BIND_SHADER_RESOURCE);

    D3D11PyramidProcessor pyramid;
    pyramid.Initialize(fx.processing);
    auto dst = pyramid.CreateDownsampledTexture(*core, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM);

    PyramidDownsampleDesc desc = {};
    pyramid.DispatchDownsample2x(core->GetImmediateContext(), src, dst, desc);

    const auto got = ReadbackRgba8(*core, dst);
    const std::vector<uint8_t> expected = {
        10, 11, 12, 255,   40, 41, 42, 255,
        70, 71, 72, 255,   100, 101, 102, 255,
    };

    CHECK_EQ(got.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        RequireByteNear(got[i], expected[i], 1, "downsample2x", i);
    }
}

TEST(ProcessingPyramid, Upsample2xPointReadback) {
    REQUIRE_CORE(core);

    Fixture fx(core);

    if (!fx.processing.SupportsRgba8Uav()) {
        std::cout << "R8G8B8A8 UAV not supported; skipping\n";
        return;
    }

    const std::vector<uint8_t> srcPixels = {
        10, 20, 30, 255,   40, 50, 60, 128,
        70, 80, 90, 255,   100, 110, 120, 64,
    };

    auto src = CreateTexture2DFromRGBA(*core, srcPixels.data(), 2, 2, D3D11_BIND_SHADER_RESOURCE);

    D3D11PyramidProcessor pyramid;
    pyramid.Initialize(fx.processing);
    auto dst = pyramid.CreateUpsampledTexture(*core, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM);

    PyramidUpsampleDesc desc = {};
    desc.filter = ProcessingFilter::Point;
    pyramid.DispatchUpsample2x(core->GetImmediateContext(), src, dst, desc);

    const auto got = ReadbackRgba8(*core, dst);
    const auto expected = MakePointUpsampleExpected2x2To4x4(srcPixels);

    CHECK_EQ(got.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        RequireByteNear(got[i], expected[i], 1, "upsample2x point", i);
    }
}

TEST(ProcessingPyramid, DownsampleOddSizeClampReadback) {
    REQUIRE_CORE(core);

    Fixture fx(core);

    if (!fx.processing.SupportsRgba8Uav()) {
        std::cout << "R8G8B8A8 UAV not supported; skipping\n";
        return;
    }

    const std::vector<uint8_t> srcPixels = {
        10, 0, 0, 255,   20, 0, 0, 255,   30, 0, 0, 255,
        40, 0, 0, 255,   50, 0, 0, 255,   60, 0, 0, 255,
        70, 0, 0, 255,   80, 0, 0, 255,   90, 0, 0, 255,
    };

    auto src = CreateTexture2DFromRGBA(*core, srcPixels.data(), 3, 3, D3D11_BIND_SHADER_RESOURCE);

    D3D11PyramidProcessor pyramid;
    pyramid.Initialize(fx.processing);
    auto dst = pyramid.CreateDownsampledTexture(*core, 3, 3, DXGI_FORMAT_R8G8B8A8_UNORM);

    PyramidDownsampleDesc desc = {};
    desc.edgeMode = PyramidEdgeMode::Clamp;
    pyramid.DispatchDownsample2x(core->GetImmediateContext(), src, dst, desc);

    const auto got = ReadbackRgba8(*core, dst);

    // 2x2 blocks with clamp on the odd right/bottom edge:
    // top-left: avg(10,20,40,50)=30
    // top-right: avg(30,30,60,60)=45
    // bottom-left: avg(70,80,70,80)=75
    // bottom-right: avg(90,90,90,90)=90
    const std::vector<uint8_t> expectedR = { 30, 45, 75, 90 };
    for (size_t pixel = 0; pixel < expectedR.size(); ++pixel) {
        const size_t i = pixel * 4u;
        RequireByteNear(got[i + 0], expectedR[pixel], 1, "odd downsample r", i + 0);
        RequireByteNear(got[i + 1], 0, 1, "odd downsample g", i + 1);
        RequireByteNear(got[i + 2], 0, 1, "odd downsample b", i + 2);
        CHECK_EQ(got[i + 3], 255);
    }
}
