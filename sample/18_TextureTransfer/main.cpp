//
// 18_TextureTransfer / main.cpp
//
// Demonstrates D3D11CpuImage <-> Texture2D transfer without file I/O.
// Higher-level libraries can use D3D11CpuImage as the bridge to PNG/JPEG/DDS
// or video encoders, but D3D11Helper itself stays DirectX/DXGI-only.
//
#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>

using namespace D3D11CoreLib;

namespace {

D3D11CpuImage MakeGradient(UINT width, UINT height) {
    auto image = CreateCpuImage(width, height, DXGI_FORMAT_R8G8B8A8_UNORM);

    for (UINT y = 0; y < height; ++y) {
        uint8_t* row = image.pixels.data() + y * image.planes[0].rowPitch;
        for (UINT x = 0; x < width; ++x) {
            row[x * 4 + 0] = static_cast<uint8_t>((x * 255) / (width - 1));
            row[x * 4 + 1] = static_cast<uint8_t>((y * 255) / (height - 1));
            row[x * 4 + 2] = static_cast<uint8_t>((x + y) & 0xFF);
            row[x * 4 + 3] = 255;
        }
    }

    return image;
}

bool EqualPackedPixels(const D3D11CpuImage& a, const D3D11CpuImage& b) {
    if (a.width != b.width || a.height != b.height || a.format != b.format) return false;
    const UINT rowBytes = GetPackedRowPitch(a.width, a.format);

    for (UINT y = 0; y < a.height; ++y) {
        const uint8_t* ar = a.pixels.data() + y * a.planes[0].rowPitch;
        const uint8_t* br = b.pixels.data() + y * b.planes[0].rowPitch;
        if (!std::equal(ar, ar + rowBytes, br)) return false;
    }
    return true;
}

} // namespace

int main() {
    try {
        D3D11CoreConfig cfg;
        cfg.allowWarpAdapter = true;
        auto core = D3D11Core::CreateShared(cfg);

        auto cpu = MakeGradient(128, 64);

        auto texture = CreateTexture2DFromCpuImage(
            *core,
            cpu,
            D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
            D3D11_USAGE_DEFAULT);

        auto readback = ReadbackTexture2DToCpuImage(*core, texture);
        if (!EqualPackedPixels(cpu, readback)) {
            throw std::runtime_error("readback pixels do not match source CPU image");
        }

        auto updated = MakeGradient(128, 64);
        for (auto& v : updated.pixels) {
            v = static_cast<uint8_t>(255 - v);
        }

        UpdateTexture2DFromCpuImage(*core, texture, updated);
        auto readbackUpdated = ReadbackTexture2DToCpuImage(*core, texture);
        if (!EqualPackedPixels(updated, readbackUpdated)) {
            throw std::runtime_error("updated readback pixels do not match updated CPU image");
        }

        std::cout << "RESULT: OK" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "RESULT: NG - " << e.what() << std::endl;
        return 1;
    }
}
