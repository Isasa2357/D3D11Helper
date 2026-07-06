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
    if (std::filesystem::exists(runtimeDir / "PyramidDownsample2xRgba.hlsl")) {
        return runtimeDir;
    }

#ifdef D3D11HELPER_SAMPLE_SOURCE_DIR
    const auto sourceDir = std::filesystem::u8path(D3D11HELPER_SAMPLE_SOURCE_DIR).parent_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(sourceDir / "PyramidDownsample2xRgba.hlsl")) {
        return sourceDir;
    }
#endif

    return runtimeDir;
}

std::vector<uint8_t> MakeCheckerRgba(UINT width, UINT height) {
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4u, 0);
    for (UINT y = 0; y < height; ++y) {
        for (UINT x = 0; x < width; ++x) {
            const bool bright = ((x / 8u) + (y / 8u)) % 2u == 0u;
            const size_t i = static_cast<size_t>(y * width + x) * 4u;
            pixels[i + 0] = bright ? 220 : 40;
            pixels[i + 1] = bright ? 220 : 40;
            pixels[i + 2] = bright ? 220 : 40;
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

        constexpr UINT width = 64;
        constexpr UINT height = 64;

        const auto pixels = MakeCheckerRgba(width, height);
        auto src = CreateTexture2DFromRGBA(
            *core,
            pixels.data(),
            width,
            height,
            D3D11_BIND_SHADER_RESOURCE);

        D3D11PyramidProcessor pyramid;
        pyramid.Initialize(processing);

        auto half = pyramid.CreateDownsampledTexture(*core, width, height, DXGI_FORMAT_R8G8B8A8_UNORM);
        auto restored = pyramid.CreateUpsampledTexture(*core, width / 2u, height / 2u, DXGI_FORMAT_R8G8B8A8_UNORM);

        PyramidDownsampleDesc down = {};
        pyramid.DispatchDownsample2x(core->GetImmediateContext(), src, half, down);

        PyramidUpsampleDesc up = {};
        up.filter = ProcessingFilter::Linear;
        pyramid.DispatchUpsample2x(core->GetImmediateContext(), half, restored, up);

        core->Flush();

        std::cout << "RESULT: OK - pyramid downsample/upsample dispatch completed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
