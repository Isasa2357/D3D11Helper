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

std::vector<uint8_t> MakeImpulse(UINT width, UINT height) {
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4u, 0);
    const UINT cx = width / 2u;
    const UINT cy = height / 2u;
    const size_t i = static_cast<size_t>(cy * width + cx) * 4u;
    pixels[i + 0] = 255;
    pixels[i + 1] = 255;
    pixels[i + 2] = 255;
    pixels[i + 3] = 255;
    return pixels;
}

uint8_t RedAt(const std::vector<uint8_t>& pixels, UINT width, UINT x, UINT y) {
    return pixels[(static_cast<size_t>(y) * width + x) * 4u + 0u];
}

} // namespace

TEST(ProcessingPyramidBlur, PyramidBlurReadback) {
    REQUIRE_CORE(core);

    Fixture fx(core);

    if (!fx.processing.SupportsRgba8Uav()) {
        std::cout << "R8G8B8A8 UAV not supported; skipping\n";
        return;
    }

    constexpr UINT width = 32;
    constexpr UINT height = 32;
    constexpr UINT levels = 2;

    const auto pixels = MakeImpulse(width, height);
    auto src = CreateTexture2DFromRGBA(*core, pixels.data(), width, height, D3D11_BIND_SHADER_RESOURCE);

    D3D11PyramidBlur blur;
    blur.Initialize(fx.processing);

    auto workspace = blur.CreateWorkspace(*core, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, levels);
    auto dst = blur.CreateOutputTexture(*core, width, height, DXGI_FORMAT_R8G8B8A8_UNORM);

    PyramidBlurDesc desc = {};
    desc.levels = levels;
    desc.blurMode = BlurMode::Gaussian;
    desc.blurRadius = 3;
    desc.blurSigma = 1.5f;
    desc.upsampleFilter = ProcessingFilter::Linear;

    blur.DispatchPyramidBlur(core->GetImmediateContext(), src, workspace, dst, desc);

    const auto got = ReadbackRgba8(*core, dst);
    CHECK_EQ(got.size(), static_cast<size_t>(width) * height * 4u);

    const uint8_t center = RedAt(got, width, width / 2u, height / 2u);
    CHECK(center < 255);
    CHECK(center > 0);
}

TEST(ProcessingPyramidBlur, PyramidRegionBlurKeepsInsideRegionSharper) {
    REQUIRE_CORE(core);

    Fixture fx(core);

    if (!fx.processing.SupportsRgba8Uav()) {
        std::cout << "R8G8B8A8 UAV not supported; skipping\n";
        return;
    }

    constexpr UINT width = 32;
    constexpr UINT height = 32;
    constexpr UINT levels = 2;

    const auto pixels = MakeImpulse(width, height);
    auto src = CreateTexture2DFromRGBA(*core, pixels.data(), width, height, D3D11_BIND_SHADER_RESOURCE);

    D3D11PyramidRegionBlur regionBlur;
    regionBlur.Initialize(fx.processing);

    auto workspace = regionBlur.CreateWorkspace(*core, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, levels);
    auto dst = regionBlur.CreateOutputTexture(*core, width, height, DXGI_FORMAT_R8G8B8A8_UNORM);

    PyramidRegionBlurDesc desc = {};
    desc.levels = levels;
    desc.shape = RegionShape::Circle;
    desc.selection = RegionSelection::Outside;
    desc.centerX = width * 0.5f;
    desc.centerY = height * 0.5f;
    desc.radius = 6.0f;
    desc.edgeSoftness = 0.0f;
    desc.blurStrength = 1.0f;
    desc.blurRadius = 3;
    desc.blurSigma = 1.5f;
    desc.upsampleFilter = ProcessingFilter::Linear;

    regionBlur.DispatchPyramidRegionBlur(core->GetImmediateContext(), src, workspace, dst, desc);

    const auto got = ReadbackRgba8(*core, dst);
    CHECK_EQ(got.size(), static_cast<size_t>(width) * height * 4u);

    // The center impulse is inside the unblurred circle and should remain intact.
    CHECK_EQ(RedAt(got, width, width / 2u, height / 2u), 255);
}
