#include "TestCommon.hpp"

#include <D3D11Helper/D3D11Core/ThrowIfFailed.hpp>
#include <D3D11Helper/D3D11Framework/D3D11Framework.hpp>
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
    if (std::filesystem::exists(runtimeDir / "ColorAdjustRgba.hlsl")) {
        return runtimeDir;
    }

#ifdef D3D11HELPER_TEST_SOURCE_DIR
    const auto sourceDir = std::filesystem::u8path(D3D11HELPER_TEST_SOURCE_DIR).parent_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(sourceDir / "ColorAdjustRgba.hlsl")) {
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

} // namespace

TEST(ProcessingColorAdjust, ShaderCompile) {
    REQUIRE_CORE(core);

    Fixture fx(core);

    D3D11ProcessingShaderCache cache;
    cache.Initialize(fx.processing);

    CHECK(!cache.GetComputeShader("ColorAdjustRgba.hlsl").Empty());
}

TEST(ProcessingColorAdjust, IdentityCopies) {
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

    D3D11ColorAdjuster adjuster;
    adjuster.Initialize(fx.processing);
    auto dst = adjuster.CreateOutputTexture(*core, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM);

    ColorAdjustDesc desc = {};
    adjuster.DispatchColorAdjust(core->GetImmediateContext(), src, dst, desc);

    const auto got = ReadbackRgba8(*core, dst);
    CHECK_EQ(got.size(), srcPixels.size());
    for (size_t i = 0; i < srcPixels.size(); ++i) {
        RequireByteNear(got[i], srcPixels[i], 1, "identity color adjust", i);
    }
}

TEST(ProcessingColorAdjust, BrightnessAndSaturationReadback) {
    REQUIRE_CORE(core);

    Fixture fx(core);

    if (!fx.processing.SupportsRgba8Uav()) {
        std::cout << "R8G8B8A8 UAV not supported; skipping\n";
        return;
    }

    const std::vector<uint8_t> srcPixels = {
        100, 100, 100, 255,
    };

    auto src = CreateTexture2DFromRGBA(*core, srcPixels.data(), 1, 1, D3D11_BIND_SHADER_RESOURCE);

    D3D11ColorAdjuster adjuster;
    adjuster.Initialize(fx.processing);
    auto dst = adjuster.CreateOutputTexture(*core, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);

    ColorAdjustDesc desc = {};
    desc.brightness = 0.2f;
    desc.saturation = 0.0f;

    adjuster.DispatchColorAdjust(core->GetImmediateContext(), src, dst, desc);

    const auto got = ReadbackRgba8(*core, dst);
    RequireByteNear(got[0], 151, 2, "brightness r", 0);
    RequireByteNear(got[1], 151, 2, "brightness g", 1);
    RequireByteNear(got[2], 151, 2, "brightness b", 2);
    CHECK_EQ(got[3], 255);
}
