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
        constexpr UINT levels = 3;

        const auto pixels = MakeImpulse(width, height);
        auto src = CreateTexture2DFromRGBA(*core, pixels.data(), width, height, D3D11_BIND_SHADER_RESOURCE);

        D3D11PyramidRegionBlur processor;
        processor.Initialize(processing);

        auto workspace = processor.CreateWorkspace(*core, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, levels);
        auto dst = processor.CreateOutputTexture(*core, width, height, DXGI_FORMAT_R8G8B8A8_UNORM);

        PyramidRegionBlurDesc desc = {};
        desc.levels = levels;
        desc.shape = RegionShape::Circle;
        desc.selection = RegionSelection::Outside;
        desc.centerX = width * 0.5f;
        desc.centerY = height * 0.5f;
        desc.radius = 24.0f;
        desc.edgeSoftness = 12.0f;
        desc.blurStrength = 1.0f;
        desc.blurRadius = 6;
        desc.blurSigma = 3.0f;
        desc.upsampleFilter = ProcessingFilter::Linear;

        processor.DispatchPyramidRegionBlur(core->GetImmediateContext(), src, workspace, dst, desc);
        core->Flush();

        std::cout << "RESULT: OK - pyramid accelerated region blur dispatch completed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
