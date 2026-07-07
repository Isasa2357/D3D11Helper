//
// 23_ViewState - console sample for v1.7.0 view / state helpers
//
#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>

#include <exception>
#include <iostream>

using namespace D3D11CoreLib;

namespace {

ComPtr<ID3D11Texture2D> CreateTexture2D(
    ID3D11Device* device,
    UINT width,
    UINT height,
    DXGI_FORMAT format,
    UINT bindFlags,
    UINT arraySize = 1,
    UINT mipLevels = 1,
    UINT miscFlags = 0) {
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = mipLevels;
    desc.ArraySize = arraySize;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = bindFlags;
    desc.MiscFlags = miscFlags;

    ComPtr<ID3D11Texture2D> texture;
    D3D11CORE_THROW_IF_FAILED(device->CreateTexture2D(&desc, nullptr, &texture));
    return texture;
}

ComPtr<ID3D11Buffer> CreateBuffer(
    ID3D11Device* device,
    UINT byteWidth,
    UINT bindFlags,
    UINT miscFlags = 0,
    UINT structureByteStride = 0) {
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = byteWidth;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = bindFlags;
    desc.MiscFlags = miscFlags;
    desc.StructureByteStride = structureByteStride;

    ComPtr<ID3D11Buffer> buffer;
    D3D11CORE_THROW_IF_FAILED(device->CreateBuffer(&desc, nullptr, &buffer));
    return buffer;
}

} // namespace

int main() {
    try {
        D3D11CoreConfig config{};
        config.enableDebugLayer = true;
        config.allowWarpAdapter = true;

        D3D11Core core;
        core.Initialize(config);
        ID3D11Device* device = core.GetDevice();

        auto textureArray = CreateTexture2D(
            device,
            8,
            8,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
            2);
        auto arraySrv = CreateTexture2DArraySrv(device, textureArray.Get());
        auto arrayRtv = CreateTexture2DArrayRtv(device, textureArray.Get());
        std::cout << "Texture2D array SRV/RTV created\n";

        auto cubeTexture = CreateTexture2D(
            device,
            4,
            4,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            D3D11_BIND_SHADER_RESOURCE,
            6,
            1,
            D3D11_RESOURCE_MISC_TEXTURECUBE);
        auto cubeSrv = CreateTextureCubeSrv(device, cubeTexture.Get());
        std::cout << "Texture cube SRV created\n";

        auto structuredBuffer = CreateBuffer(
            device,
            16 * 4,
            D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
            D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
            16);
        auto structuredSrv = CreateStructuredBufferSrv(device, structuredBuffer.Get(), 0, 4);
        auto structuredUav = CreateStructuredBufferUav(device, structuredBuffer.Get(), 0, 4);
        std::cout << "Structured buffer SRV/UAV created\n";

        auto rawBuffer = CreateBuffer(
            device,
            64,
            D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
            D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS);
        auto rawSrv = CreateRawBufferSrv(device, rawBuffer.Get(), 0, 16);
        auto rawUav = CreateRawBufferUav(device, rawBuffer.Get(), 0, 16);
        std::cout << "Raw buffer SRV/UAV created\n";

        auto sampler = CreateSamplerState(device, StatePresets::SamplerLinearClamp());
        auto rasterizer = CreateRasterizerState(device, StatePresets::RasterizerCullBack());
        auto blend = CreateBlendState(device, StatePresets::BlendAlpha());
        auto depth = CreateDepthStencilState(device, StatePresets::DepthDefault());
        std::cout << "State objects created\n";

        (void)arraySrv;
        (void)arrayRtv;
        (void)cubeSrv;
        (void)structuredSrv;
        (void)structuredUav;
        (void)rawSrv;
        (void)rawUav;
        (void)sampler;
        (void)rasterizer;
        (void)blend;
        (void)depth;

        std::cout << "RESULT: OK\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ViewState sample failed: " << e.what() << "\n";
        return 1;
    }
}
