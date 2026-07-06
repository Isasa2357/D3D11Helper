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
    if (std::filesystem::exists(runtimeDir / "GaussianBlurHorizontalRgba.hlsl")) {
        return runtimeDir;
    }

#ifdef D3D11HELPER_TEST_SOURCE_DIR
    const auto sourceDir = std::filesystem::u8path(D3D11HELPER_TEST_SOURCE_DIR).parent_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(sourceDir / "GaussianBlurHorizontalRgba.hlsl")) {
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

std::vector<uint8_t> MakeCenterImpulse3x3() {
    std::vector<uint8_t> pixels(3u * 3u * 4u, 0);

    const size_t center = static_cast<size_t>(1u * 3u + 1u) * 4u;
    pixels[center + 0] = 255;
    pixels[center + 1] = 0;
    pixels[center + 2] = 0;
    pixels[center + 3] = 255;

    return pixels;
}

} // namespace

TEST(ProcessingBlur, ShaderCompile) {
    REQUIRE_CORE(core);

    Fixture fx(core);

    D3D11ProcessingShaderCache cache;
    cache.Initialize(fx.processing);

    CHECK(!cache.GetComputeShader("GaussianBlurHorizontalRgba.hlsl").Empty());
    CHECK(!cache.GetComputeShader("GaussianBlurVerticalRgba.hlsl").Empty());
}

TEST(ProcessingBlur, BoxBlurImpulseReadback) {
    REQUIRE_CORE(core);

    Fixture fx(core);

    if (!fx.processing.SupportsRgba8Uav()) {
        std::cout << "R8G8B8A8 UAV not supported; skipping\n";
        return;
    }

    const auto srcPixels = MakeCenterImpulse3x3();
    auto src = CreateTexture2DFromRGBA(
        *core,
        srcPixels.data(),
        3,
        3,
        D3D11_BIND_SHADER_RESOURCE);

    D3D11Blurrer blurrer;
    blurrer.Initialize(fx.processing);

    auto scratch = blurrer.CreateScratchTexture(*core, 3, 3, DXGI_FORMAT_R8G8B8A8_UNORM);
    auto dst = blurrer.CreateOutputTexture(*core, 3, 3, DXGI_FORMAT_R8G8B8A8_UNORM);

    BlurDesc desc = {};
    desc.mode = BlurMode::Box;
    desc.radius = 1;
    desc.edgeMode = BlurEdgeMode::Constant;
    desc.borderColor[0] = 0.0f;
    desc.borderColor[1] = 0.0f;
    desc.borderColor[2] = 0.0f;
    desc.borderColor[3] = 0.0f;

    blurrer.DispatchBlur(core->GetImmediateContext(), src, scratch, dst, desc);

    const auto got = ReadbackRgba8(*core, dst);

    for (size_t i = 0; i < got.size(); i += 4) {
        RequireByteNear(got[i + 0], 28, 2, "box blur r", i + 0);
        RequireByteNear(got[i + 1], 0, 1, "box blur g", i + 1);
        RequireByteNear(got[i + 2], 0, 1, "box blur b", i + 2);
        RequireByteNear(got[i + 3], 28, 2, "box blur a", i + 3);
    }
}

TEST(ProcessingBlur, GaussianBlurRadiusZeroCopies) {
    REQUIRE_CORE(core);

    Fixture fx(core);

    if (!fx.processing.SupportsRgba8Uav()) {
        std::cout << "R8G8B8A8 UAV not supported; skipping\n";
        return;
    }

    const std::vector<uint8_t> srcPixels = {
        10, 20, 30, 255,   40, 50, 60, 255,
        70, 80, 90, 255,   100, 110, 120, 255,
    };

    auto src = CreateTexture2DFromRGBA(
        *core,
        srcPixels.data(),
        2,
        2,
        D3D11_BIND_SHADER_RESOURCE);

    D3D11Blurrer blurrer;
    blurrer.Initialize(fx.processing);

    auto scratch = blurrer.CreateScratchTexture(*core, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM);
    auto dst = blurrer.CreateOutputTexture(*core, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM);

    BlurDesc desc = {};
    desc.mode = BlurMode::Gaussian;
    desc.radius = 0;
    desc.sigma = 1.0f;

    blurrer.DispatchBlur(core->GetImmediateContext(), src, scratch, dst, desc);

    const auto got = ReadbackRgba8(*core, dst);
    CHECK_EQ(got.size(), srcPixels.size());

    for (size_t i = 0; i < srcPixels.size(); ++i) {
        CHECK_EQ(got[i], srcPixels[i]);
    }
}
