#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>
#include <D3D11Helper/D3D11Processing/D3D11Processing.hpp>

#include <filesystem>
#include <iostream>
#include <vector>
#include <utility>

using namespace D3D11CoreLib;
using namespace D3D11CoreLib::Processing;

namespace {
std::filesystem::path ProcessingShaderDir() {
    const auto runtimeDir = std::filesystem::current_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(runtimeDir / "ConvertYuv420ToRgb.hlsl")) return runtimeDir;
#ifdef D3D11HELPER_SAMPLE_SOURCE_DIR
    const auto sourceDir = std::filesystem::u8path(D3D11HELPER_SAMPLE_SOURCE_DIR).parent_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(sourceDir / "ConvertYuv420ToRgb.hlsl")) return sourceDir;
#endif
    return runtimeDir;
}
D3D11Resource CreateP010(ID3D11Device* device, UINT w, UINT h) {
    std::vector<uint16_t> data(static_cast<size_t>(w)*h + static_cast<size_t>(w)*(h/2), 32768);
    D3D11_TEXTURE2D_DESC desc = {}; desc.Width=w; desc.Height=h; desc.MipLevels=1; desc.ArraySize=1; desc.Format=DXGI_FORMAT_P010; desc.SampleDesc.Count=1; desc.Usage=D3D11_USAGE_DEFAULT; desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init = {}; init.pSysMem=data.data(); init.SysMemPitch=w*sizeof(uint16_t);
    ComPtr<ID3D11Texture2D> tex; D3D11CORE_THROW_IF_FAILED(device->CreateTexture2D(&desc, &init, &tex)); return D3D11Resource(std::move(tex));
}
}
int main() {
    try {
        D3D11CoreConfig cfg; cfg.allowWarpAdapter = true;
        auto core = D3D11Core::CreateShared(cfg);
        D3D11ProcessingContext processing; processing.Initialize(*core, ProcessingShaderDir());
        if (!processing.SupportsP010Srv() || !processing.SupportsRgba16FloatUav()) { std::cout << "Required P010/RGBA16F support unavailable.\n"; return 0; }
        auto src = CreateP010(core->GetDevice(), 4, 4);
        D3D11FormatConverter conv; conv.Initialize(processing);
        auto dst = conv.CreateOutputTexture(*core, 4, 4, DXGI_FORMAT_R16G16B16A16_FLOAT);
        FormatConvertDesc desc = {}; desc.srcFormat=DXGI_FORMAT_P010; desc.dstFormat=DXGI_FORMAT_R16G16B16A16_FLOAT; desc.color.srcRange=ProcessingColorRange::Full;
        conv.DispatchConvert(core->GetImmediateContext(), src, dst, desc);
        core->Flush();
        std::cout << "RESULT: OK - P010 -> RGBA16F conversion dispatch completed.\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "ERROR: " << e.what() << "\n"; return 1; }
}
