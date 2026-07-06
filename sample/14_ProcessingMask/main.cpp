#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Framework/D3D11Framework.hpp>
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
    if (std::filesystem::exists(runtimeDir / "MaskApplyRgba.hlsl")) return runtimeDir;
#ifdef D3D11HELPER_SAMPLE_SOURCE_DIR
    const auto sourceDir = std::filesystem::u8path(D3D11HELPER_SAMPLE_SOURCE_DIR).parent_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(sourceDir / "MaskApplyRgba.hlsl")) return sourceDir;
#endif
    return runtimeDir;
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

        const std::vector<uint8_t> srcPixels = {
            255, 0, 0, 255,   0, 255, 0, 255,
            0, 0, 255, 255,   255, 255, 255, 255,
        };
        const std::vector<uint8_t> maskPixels = {
            0, 0, 0, 0,       0, 0, 0, 128,
            0, 0, 0, 192,     0, 0, 0, 255,
        };

        auto src = CreateTexture2DFromRGBA(*core, srcPixels.data(), 2, 2, D3D11_BIND_SHADER_RESOURCE);
        auto mask = CreateTexture2DFromRGBA(*core, maskPixels.data(), 2, 2, D3D11_BIND_SHADER_RESOURCE);

        D3D11MaskProcessor masks;
        masks.Initialize(processing);
        auto dst = masks.CreateOutputTexture(*core, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM);

        MaskApplyDesc desc = {};
        desc.mode = MaskApplyMode::ApplyAlpha;
        desc.channel = MaskChannel::Alpha;
        masks.DispatchApplyMask(core->GetImmediateContext(), src, mask, dst, desc);
        core->Flush();

        std::cout << "RESULT: OK - alpha mask dispatch completed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
