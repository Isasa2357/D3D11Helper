#include "TestCommon.hpp"

#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
#include <D3D11Helper/D3D11Processing/D3D11Processing.hpp>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <utility>
#include <vector>

using namespace D3D11CoreLib;
using namespace D3D11CoreLib::Processing;

namespace {

bool HasProcessingShader(const std::filesystem::path& dir) {
    std::error_code ec;
    return std::filesystem::exists(dir / "RegionBlurBlendRgba.hlsl", ec) && !ec;
}

std::filesystem::path ProcessingShaderDir() {
    const auto namespacedRuntimeDir = std::filesystem::current_path() / "D3D11Helper" / "shaders" / "D3D11Processing";
    if (HasProcessingShader(namespacedRuntimeDir)) {
        return namespacedRuntimeDir;
    }

    const auto legacyRuntimeDir = std::filesystem::current_path() / "shaders" / "D3D11Processing";
    if (HasProcessingShader(legacyRuntimeDir)) {
        return legacyRuntimeDir;
    }

#ifdef D3D11HELPER_TEST_SOURCE_DIR
    const auto sourceDir = std::filesystem::u8path(D3D11HELPER_TEST_SOURCE_DIR).parent_path() / "shaders" / "D3D11Processing";
    if (HasProcessingShader(sourceDir)) {
        return sourceDir;
    }
#endif

    return namespacedRuntimeDir;
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

PyramidRegionBlurDesc MakeValidDesc(UINT width, UINT height, UINT levels) {
    PyramidRegionBlurDesc desc = {};
    desc.levels = levels;
    desc.shape = RegionShape::Circle;
    desc.selection = RegionSelection::Outside;
    desc.centerX = width * 0.5f;
    desc.centerY = height * 0.5f;
    desc.radius = 6.0f;
    desc.edgeSoftness = 0.0f;
    desc.blurStrength = 1.0f;
    desc.blurMode = BlurMode::Gaussian;
    desc.blurRadius = 3;
    desc.blurSigma = 1.5f;
    desc.upsampleFilter = ProcessingFilter::Linear;
    return desc;
}

} // namespace

TEST(ProcessingPyramidRegionBlur, ShaderCompile) {
    REQUIRE_CORE(core);
    Fixture fx(core);

    D3D11ProcessingShaderCache cache;
    cache.Initialize(fx.processing);

    CHECK(!cache.GetComputeShader("PyramidDownsample2xRgba.hlsl").Empty());
    CHECK(!cache.GetComputeShader("PyramidUpsample2xRgba.hlsl").Empty());
    CHECK(!cache.GetComputeShader("GaussianBlurHorizontalRgba.hlsl").Empty());
    CHECK(!cache.GetComputeShader("GaussianBlurVerticalRgba.hlsl").Empty());
    CHECK(!cache.GetComputeShader("RegionBlurBlendRgba.hlsl").Empty());
}

TEST(ProcessingPyramidRegionBlur, CircleOutsideKeepsInsideRegionSharp) {
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

    auto desc = MakeValidDesc(width, height, levels);
    regionBlur.DispatchPyramidRegionBlur(core->GetImmediateContext(), src, workspace, dst, desc);

    const auto got = ReadbackRgba8(*core, dst);
    CHECK_EQ(got.size(), static_cast<size_t>(width) * height * 4u);
    CHECK_EQ(RedAt(got, width, width / 2u, height / 2u), 255);
}

TEST(ProcessingPyramidRegionBlur, RejectsInvalidCircleRadius) {
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

    auto desc = MakeValidDesc(width, height, levels);
    desc.radius = 0.0f;

    CHECK_THROWS(regionBlur.DispatchPyramidRegionBlur(core->GetImmediateContext(), src, workspace, dst, desc));
}
