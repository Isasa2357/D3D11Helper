#include "TestCommon.hpp"

#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
#include <D3D11Helper/D3D11Processing/D3D11Processing.hpp>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

using namespace D3D11CoreLib;
using namespace D3D11CoreLib::Processing;

namespace {

std::filesystem::path ProcessingShaderDir() {
    const auto runtimeDir = std::filesystem::current_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(runtimeDir / "RegionEffectRgba.hlsl")) {
        return runtimeDir;
    }

#ifdef D3D11HELPER_TEST_SOURCE_DIR
    const auto sourceDir = std::filesystem::u8path(D3D11HELPER_TEST_SOURCE_DIR).parent_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(sourceDir / "RegionEffectRgba.hlsl")) {
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

std::vector<uint8_t> MakeSolidRgba(UINT width, UINT height, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4u, 0);
    for (size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i + 0] = r;
        pixels[i + 1] = g;
        pixels[i + 2] = b;
        pixels[i + 3] = a;
    }
    return pixels;
}

} // namespace

TEST(ProcessingRegionEffect, ShaderCompile) {
    REQUIRE_CORE(core);

    Fixture fx(core);

    D3D11ProcessingShaderCache cache;
    cache.Initialize(fx.processing);

    CHECK(!cache.GetComputeShader("RegionEffectRgba.hlsl").Empty());
}

TEST(ProcessingRegionEffect, DarkenOutsideCircleReadback) {
    REQUIRE_CORE(core);

    Fixture fx(core);

    if (!fx.processing.SupportsRgba8Uav()) {
        std::cout << "R8G8B8A8 UAV not supported; skipping\n";
        return;
    }

    constexpr UINT width = 4;
    constexpr UINT height = 4;

    const auto srcPixels = MakeSolidRgba(width, height, 100, 120, 140, 255);
    auto src = CreateTexture2DFromRGBA(
        *core,
        srcPixels.data(),
        width,
        height,
        D3D11_BIND_SHADER_RESOURCE);

    D3D11RegionEffect region;
    region.Initialize(fx.processing);

    auto dst = region.CreateOutputTexture(*core, width, height, DXGI_FORMAT_R8G8B8A8_UNORM);

    RegionEffectDesc desc = {};
    desc.effect = RegionEffectMode::Darken;
    desc.shape = RegionShape::Circle;
    desc.selection = RegionSelection::Outside;
    desc.centerX = 2.0f;
    desc.centerY = 2.0f;
    desc.radius = 1.1f;
    desc.edgeSoftness = 0.0f;
    desc.darkenFactor = 0.5f;

    region.DispatchRegionEffect(core->GetImmediateContext(), src, dst, desc);

    const auto got = ReadbackRgba8(*core, dst);

    for (UINT y = 0; y < height; ++y) {
        for (UINT x = 0; x < width; ++x) {
            const size_t i = static_cast<size_t>(y * width + x) * 4u;
            const bool inside = (x == 1 || x == 2) && (y == 1 || y == 2);
            const uint8_t r = inside ? 100 : 50;
            const uint8_t g = inside ? 120 : 60;
            const uint8_t b = inside ? 140 : 70;
            RequireByteNear(got[i + 0], r, 1, "region r", i + 0);
            RequireByteNear(got[i + 1], g, 1, "region g", i + 1);
            RequireByteNear(got[i + 2], b, 1, "region b", i + 2);
            CHECK_EQ(got[i + 3], 255);
        }
    }
}

TEST(ProcessingRegionEffect, TintInsideRectReadback) {
    REQUIRE_CORE(core);

    Fixture fx(core);

    if (!fx.processing.SupportsRgba8Uav()) {
        std::cout << "R8G8B8A8 UAV not supported; skipping\n";
        return;
    }

    constexpr UINT width = 4;
    constexpr UINT height = 4;

    const auto srcPixels = MakeSolidRgba(width, height, 100, 100, 100, 255);
    auto src = CreateTexture2DFromRGBA(
        *core,
        srcPixels.data(),
        width,
        height,
        D3D11_BIND_SHADER_RESOURCE);

    D3D11RegionEffect region;
    region.Initialize(fx.processing);

    auto dst = region.CreateOutputTexture(*core, width, height, DXGI_FORMAT_R8G8B8A8_UNORM);

    RegionEffectDesc desc = {};
    desc.effect = RegionEffectMode::Tint;
    desc.shape = RegionShape::Rect;
    desc.selection = RegionSelection::Inside;
    desc.rectX = 1.0f;
    desc.rectY = 1.0f;
    desc.rectWidth = 2.0f;
    desc.rectHeight = 2.0f;
    desc.edgeSoftness = 0.0f;
    desc.tintColor[0] = 1.0f;
    desc.tintColor[1] = 0.0f;
    desc.tintColor[2] = 0.0f;
    desc.tintColor[3] = 1.0f;
    desc.tintStrength = 0.5f;

    region.DispatchRegionEffect(core->GetImmediateContext(), src, dst, desc);

    const auto got = ReadbackRgba8(*core, dst);

    for (UINT y = 0; y < height; ++y) {
        for (UINT x = 0; x < width; ++x) {
            const size_t i = static_cast<size_t>(y * width + x) * 4u;
            const bool inside = (x == 1 || x == 2) && (y == 1 || y == 2);
            const uint8_t r = inside ? 178 : 100;
            const uint8_t g = inside ? 50 : 100;
            const uint8_t b = inside ? 50 : 100;
            RequireByteNear(got[i + 0], r, 2, "tint r", i + 0);
            RequireByteNear(got[i + 1], g, 2, "tint g", i + 1);
            RequireByteNear(got[i + 2], b, 2, "tint b", i + 2);
            CHECK_EQ(got[i + 3], 255);
        }
    }
}
