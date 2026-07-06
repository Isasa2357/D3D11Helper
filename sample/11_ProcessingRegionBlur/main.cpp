#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
#include <D3D11Helper/D3D11Processing/D3D11Processing.hpp>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <vector>

using namespace D3D11CoreLib;
using namespace D3D11CoreLib::Processing;

namespace {

std::filesystem::path ProcessingShaderDir() {
    const auto runtimeDir = std::filesystem::current_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(runtimeDir / "RegionBlurBlendRgba.hlsl")) {
        return runtimeDir;
    }

#ifdef D3D11HELPER_SAMPLE_SOURCE_DIR
    const auto sourceDir = std::filesystem::u8path(D3D11HELPER_SAMPLE_SOURCE_DIR).parent_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(sourceDir / "RegionBlurBlendRgba.hlsl")) {
        return sourceDir;
    }
#endif

    return runtimeDir;
}

std::vector<uint8_t> MakeCheckerRgba(UINT width, UINT height) {
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4u, 0);

    for (UINT y = 0; y < height; ++y) {
        for (UINT x = 0; x < width; ++x) {
            const bool bright = (((x / 8u) + (y / 8u)) & 1u) == 0u;
            const uint8_t v = bright ? 230 : 30;
            const size_t i = static_cast<size_t>(y * width + x) * 4u;
            pixels[i + 0] = v;
            pixels[i + 1] = static_cast<uint8_t>(255u - v / 2u);
            pixels[i + 2] = static_cast<uint8_t>(80u + v / 3u);
            pixels[i + 3] = 255;
        }
    }

    return pixels;
}

} // namespace

int main() {
    try {
        D3D11CoreConfig cfg;
        cfg.allowWarpAdapter = true;

        auto core = D3D11Core::CreateShared(cfg);

        D3D11ProcessingContext processing;
        processing.Initialize(*core, ProcessingShaderDir());

        if (!processing.SupportsRgba8Uav()) {
            std::cout << "R8G8B8A8 UAV is not supported; skipping sample.\n";
            return 0;
        }

        constexpr UINT width = 128;
        constexpr UINT height = 128;

        const auto pixels = MakeCheckerRgba(width, height);
        auto src = CreateTexture2DFromRGBA(
            *core,
            pixels.data(),
            width,
            height,
            D3D11_BIND_SHADER_RESOURCE);

        D3D11RegionBlur regionBlur;
        regionBlur.Initialize(processing);

        auto blurScratch = regionBlur.CreateScratchTexture(*core, width, height, DXGI_FORMAT_R8G8B8A8_UNORM);
        auto blurred = regionBlur.CreateBlurredTexture(*core, width, height, DXGI_FORMAT_R8G8B8A8_UNORM);
        auto dst = regionBlur.CreateOutputTexture(*core, width, height, DXGI_FORMAT_R8G8B8A8_UNORM);

        RegionBlurDesc desc = {};
        desc.shape = RegionShape::Circle;
        desc.selection = RegionSelection::Outside;
        desc.centerX = width * 0.5f;
        desc.centerY = height * 0.5f;
        desc.radius = 32.0f;
        desc.edgeSoftness = 12.0f;
        desc.blurStrength = 1.0f;
        desc.blurMode = BlurMode::Gaussian;
        desc.blurRadius = 8;
        desc.blurSigma = 3.0f;
        desc.blurEdgeMode = BlurEdgeMode::Clamp;

        regionBlur.DispatchRegionBlur(core->GetImmediateContext(), src, blurScratch, blurred, dst, desc);
        core->Flush();

        std::cout << "RESULT: OK - outside-circle region blur dispatch completed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
