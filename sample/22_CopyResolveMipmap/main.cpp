//
// 22_CopyResolveMipmap - console copy / resolve / mipmap sample
//
#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>

#include <cstdint>
#include <exception>
#include <iostream>
#include <vector>

using namespace D3D11CoreLib;

namespace {

ComPtr<ID3D11Texture2D> CreateTexture2D(ID3D11Device* device, UINT width, UINT height, DXGI_FORMAT format, UINT bindFlags, UINT sampleCount = 1, UINT miscFlags = 0, UINT mipLevels = 1, const void* initialPixels = nullptr) {
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = mipLevels;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = sampleCount;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = bindFlags;
    desc.MiscFlags = miscFlags;

    D3D11_SUBRESOURCE_DATA init{};
    D3D11_SUBRESOURCE_DATA* initPtr = nullptr;
    if (initialPixels) {
        init.pSysMem = initialPixels;
        init.SysMemPitch = width * 4;
        initPtr = &init;
    }

    ComPtr<ID3D11Texture2D> texture;
    D3D11CORE_THROW_IF_FAILED(device->CreateTexture2D(&desc, initPtr, &texture));
    return texture;
}

ComPtr<ID3D11Buffer> CreateBuffer(ID3D11Device* device, UINT byteWidth, const void* initialData = nullptr) {
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = byteWidth;
    desc.Usage = D3D11_USAGE_DEFAULT;

    D3D11_SUBRESOURCE_DATA init{};
    D3D11_SUBRESOURCE_DATA* initPtr = nullptr;
    if (initialData) {
        init.pSysMem = initialData;
        initPtr = &init;
    }

    ComPtr<ID3D11Buffer> buffer;
    D3D11CORE_THROW_IF_FAILED(device->CreateBuffer(&desc, initPtr, &buffer));
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
        ID3D11DeviceContext* context = core.GetImmediateContext();

        std::vector<uint8_t> pixels(16 * 16 * 4, 0x80);
        auto srcTexture = CreateTexture2D(device, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 1, 0, 1, pixels.data());
        auto dstTexture = CreateTexture2D(device, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
        CopyTexture2D(context, dstTexture.Get(), srcTexture.Get());
        std::cout << "Texture copy completed\n";

        std::vector<uint8_t> bufferData(64, 0x42);
        auto srcBuffer = CreateBuffer(device, static_cast<UINT>(bufferData.size()), bufferData.data());
        auto dstBuffer = CreateBuffer(device, static_cast<UINT>(bufferData.size()));
        CopyBuffer(context, dstBuffer.Get(), srcBuffer.Get());
        std::cout << "Buffer copy completed\n";

        UINT quality = 0;
        D3D11CORE_THROW_IF_FAILED(device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, 4, &quality));
        if (quality > 0) {
            auto msaa = CreateTexture2D(device, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET, 4);
            auto resolved = CreateTexture2D(device, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
            ComPtr<ID3D11RenderTargetView> rtv;
            D3D11CORE_THROW_IF_FAILED(device->CreateRenderTargetView(msaa.Get(), nullptr, &rtv));
            const float clearColor[4] = { 0.1f, 0.3f, 0.8f, 1.0f };
            context->ClearRenderTargetView(rtv.Get(), clearColor);
            ResolveTexture2D(context, resolved.Get(), msaa.Get());
            std::cout << "MSAA resolve completed\n";
        } else {
            std::cout << "MSAA resolve skipped: 4x R8G8B8A8_UNORM unsupported\n";
        }

        if (IsAutoGenerateMipsSupported(device, DXGI_FORMAT_R8G8B8A8_UNORM)) {
            auto mipTexture = CreateTexture2D(
                device,
                16,
                16,
                DXGI_FORMAT_R8G8B8A8_UNORM,
                D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
                1,
                D3D11_RESOURCE_MISC_GENERATE_MIPS,
                5);
            auto srv = CreateMipGenerationTexture2DSRV(device, mipTexture.Get());
            GenerateMips(context, srv.Get());
            std::cout << "Mipmap generation completed\n";
        } else {
            std::cout << "Mipmap generation skipped: format autogen unsupported\n";
        }

        context->Flush();
        std::cout << "RESULT: OK\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Copy/Resolve/Mipmap sample failed: " << e.what() << "\n";
        return 1;
    }
}
