#include "D3D11Core/D3D11Core.hpp"
#include "D3D11Framework/D3D11Framework.hpp"
#include "D3D11Core/ThrowIfFailed.hpp"
#include "D3D11Processing/D3D11Processing.hpp"

#include <filesystem>
#include <iostream>
#include <vector>
#include <utility>

using namespace D3D11CoreLib;
using namespace D3D11CoreLib::Processing;

namespace {
std::filesystem::path ProcessingShaderDir() {
    const auto runtimeDir = std::filesystem::current_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(runtimeDir / "FusedYuv420ToRgbResize.hlsl")) return runtimeDir;
#ifdef D3D11HELPER_SAMPLE_SOURCE_DIR
    const auto sourceDir = std::filesystem::u8path(D3D11HELPER_SAMPLE_SOURCE_DIR).parent_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(sourceDir / "FusedYuv420ToRgbResize.hlsl")) return sourceDir;
#endif
    return runtimeDir;
}
D3D11Resource CreateNv12(ID3D11Device* device, UINT w, UINT h) {
    std::vector<uint8_t> data(static_cast<size_t>(w)*h + static_cast<size_t>(w)*(h/2), 128);
    D3D11_TEXTURE2D_DESC desc = {}; desc.Width=w; desc.Height=h; desc.MipLevels=1; desc.ArraySize=1; desc.Format=DXGI_FORMAT_NV12; desc.SampleDesc.Count=1; desc.Usage=D3D11_USAGE_DEFAULT; desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init = {}; init.pSysMem=data.data(); init.SysMemPitch=w;
    ComPtr<ID3D11Texture2D> tex; D3D11CORE_THROW_IF_FAILED(device->CreateTexture2D(&desc, &init, &tex)); return D3D11Resource(std::move(tex));
}
}
int main() {
    try {
        D3D11CoreConfig cfg; cfg.allowWarpAdapter = true;
        auto core = D3D11Core::CreateShared(cfg);
        D3D11ProcessingContext processing; processing.Initialize(*core, ProcessingShaderDir());
        if (!processing.SupportsNv12Srv() || !processing.SupportsRgba8Uav()) { std::cout << "Required format support unavailable.\n"; return 0; }
        auto src = CreateNv12(core->GetDevice(), 4, 4);
        D3D11FusedProcessor fused; fused.Initialize(processing);
        auto dst = fused.CreateOutputTexture(*core, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM);
        FusedConvertResizeDesc desc = {}; desc.srcFormat=DXGI_FORMAT_NV12; desc.dstFormat=DXGI_FORMAT_R8G8B8A8_UNORM; desc.filter=ProcessingFilter::Point;
        fused.DispatchConvertResize(core->GetImmediateContext(), src, dst, desc);
        core->Flush();
        std::cout << "RESULT: OK - NV12 -> RGBA resize fused dispatch completed.\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "ERROR: " << e.what() << "\n"; return 1; }
}
