//
// test_copy.cpp - D3D11 copy / resolve / mipmap helper tests
//
#include "TestUtil.hpp"

#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Copy.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Mipmap.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Resolve.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace D3D11CoreLib;

namespace {

void ExpectThrows(const char* label, const std::function<void()>& fn) {
    bool threw = false;
    try {
        fn();
    } catch (...) {
        threw = true;
    }
    if (!threw) {
        throw std::runtime_error(std::string(label) + " did not throw");
    }
}

ComPtr<ID3D11Texture2D> CreateTexture(
    ID3D11Device* device,
    UINT width,
    UINT height,
    DXGI_FORMAT format,
    UINT bindFlags,
    const std::vector<uint8_t>* pixels = nullptr,
    UINT sampleCount = 1,
    UINT miscFlags = 0,
    UINT mipLevels = 1) {
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
    if (pixels) {
        init.pSysMem = pixels->data();
        init.SysMemPitch = width * 4;
        initPtr = &init;
    }

    ComPtr<ID3D11Texture2D> texture;
    D3D11CORE_THROW_IF_FAILED(device->CreateTexture2D(&desc, initPtr, &texture));
    return texture;
}

std::vector<uint8_t> ReadTexture(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    ID3D11Texture2D* texture,
    UINT width,
    UINT height) {
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;

    ComPtr<ID3D11Texture2D> staging;
    D3D11CORE_THROW_IF_FAILED(device->CreateTexture2D(&desc, nullptr, &staging));
    context->CopyResource(staging.Get(), texture);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    D3D11CORE_THROW_IF_FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));
    std::vector<uint8_t> result(width * height * 4);
    for (UINT y = 0; y < height; ++y) {
        const auto* src = static_cast<const uint8_t*>(mapped.pData) + y * mapped.RowPitch;
        std::copy(src, src + width * 4, result.data() + y * width * 4);
    }
    context->Unmap(staging.Get(), 0);
    return result;
}

ComPtr<ID3D11Buffer> CreateBuffer(ID3D11Device* device, UINT byteWidth, const std::vector<uint8_t>* data = nullptr) {
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = byteWidth;
    desc.Usage = D3D11_USAGE_DEFAULT;

    D3D11_SUBRESOURCE_DATA init{};
    D3D11_SUBRESOURCE_DATA* initPtr = nullptr;
    if (data) {
        init.pSysMem = data->data();
        initPtr = &init;
    }

    ComPtr<ID3D11Buffer> buffer;
    D3D11CORE_THROW_IF_FAILED(device->CreateBuffer(&desc, initPtr, &buffer));
    return buffer;
}

std::vector<uint8_t> ReadBuffer(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    ID3D11Buffer* buffer,
    UINT byteWidth) {
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = byteWidth;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    ComPtr<ID3D11Buffer> staging;
    D3D11CORE_THROW_IF_FAILED(device->CreateBuffer(&desc, nullptr, &staging));
    context->CopyResource(staging.Get(), buffer);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    D3D11CORE_THROW_IF_FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));
    std::vector<uint8_t> result(byteWidth);
    std::copy(
        static_cast<const uint8_t*>(mapped.pData),
        static_cast<const uint8_t*>(mapped.pData) + byteWidth,
        result.data());
    context->Unmap(staging.Get(), 0);
    return result;
}

} // namespace

int main() {
    auto core = TestUtil::MakeCore();
    auto* device = core->GetDevice();
    auto* context = core->GetImmediateContext();

    TEST_RUN("Texture2D full copy", {
        std::vector<uint8_t> pixels(8 * 8 * 4);
        for (size_t i = 0; i < pixels.size(); ++i) {
            pixels[i] = static_cast<uint8_t>(i);
        }
        auto src = CreateTexture(device, 8, 8, DXGI_FORMAT_R8G8B8A8_UNORM, 0, &pixels);
        auto dst = CreateTexture(device, 8, 8, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
        CopyTexture2D(context, dst.Get(), src.Get());
        if (ReadTexture(device, context, dst.Get(), 8, 8) != pixels) {
            throw std::runtime_error("texture copy mismatch");
        }
    });

    TEST_RUN("Texture2D region copy", {
        std::vector<uint8_t> srcPixels(8 * 8 * 4);
        for (size_t i = 0; i < srcPixels.size(); ++i) {
            srcPixels[i] = static_cast<uint8_t>(17 + i);
        }
        std::vector<uint8_t> zeros(8 * 8 * 4, 0);
        auto src = CreateTexture(device, 8, 8, DXGI_FORMAT_R8G8B8A8_UNORM, 0, &srcPixels);
        auto dst = CreateTexture(device, 8, 8, DXGI_FORMAT_R8G8B8A8_UNORM, 0, &zeros);

        D3D11Texture2DRegion region{};
        region.srcX = 2;
        region.srcY = 1;
        region.dstX = 3;
        region.dstY = 2;
        region.width = 3;
        region.height = 4;
        CopyTexture2DRegion(context, dst.Get(), src.Get(), region);

        auto actual = ReadTexture(device, context, dst.Get(), 8, 8);
        for (UINT y = 0; y < 8; ++y) {
            for (UINT x = 0; x < 8; ++x) {
                for (UINT c = 0; c < 4; ++c) {
                    const size_t dstIndex = (y * 8 + x) * 4 + c;
                    uint8_t expected = 0;
                    if (x >= 3 && x < 6 && y >= 2 && y < 6) {
                        expected = srcPixels[((y - 2 + 1) * 8 + (x - 3 + 2)) * 4 + c];
                    }
                    if (actual[dstIndex] != expected) {
                        throw std::runtime_error("texture region mismatch");
                    }
                }
            }
        }
    });

    TEST_RUN("Buffer full and region copy", {
        std::vector<uint8_t> srcData(64), dstData(64, 0);
        for (size_t i = 0; i < srcData.size(); ++i) {
            srcData[i] = static_cast<uint8_t>(i * 3);
        }
        auto src = CreateBuffer(device, 64, &srcData);
        auto dst = CreateBuffer(device, 64, &dstData);
        CopyBuffer(context, dst.Get(), src.Get());
        if (ReadBuffer(device, context, dst.Get(), 64) != srcData) {
            throw std::runtime_error("buffer copy mismatch");
        }

        std::vector<uint8_t> zero(64, 0);
        dst = CreateBuffer(device, 64, &zero);
        D3D11BufferCopyRegion region{};
        region.srcOffset = 8;
        region.dstOffset = 20;
        region.sizeBytes = 16;
        CopyBufferRegion(context, dst.Get(), src.Get(), region);
        auto actual = ReadBuffer(device, context, dst.Get(), 64);
        for (UINT i = 0; i < 64; ++i) {
            const uint8_t expected = (i >= 20 && i < 36) ? srcData[i - 20 + 8] : 0;
            if (actual[i] != expected) {
                throw std::runtime_error("buffer region mismatch");
            }
        }
    });

    TEST_RUN("ResolveTexture2D optional MSAA", {
        UINT quality = 0;
        D3D11CORE_THROW_IF_FAILED(device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, 4, &quality));
        if (quality == 0) {
            TestUtil::Log("Skipping resolve: 4x MSAA unsupported");
            return;
        }
        auto src = CreateTexture(device, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_RENDER_TARGET, nullptr, 4);
        auto dst = CreateTexture(device, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
        ComPtr<ID3D11RenderTargetView> rtv;
        D3D11CORE_THROW_IF_FAILED(device->CreateRenderTargetView(src.Get(), nullptr, &rtv));
        const float color[4] = { 1.0f, 0.25f, 0.5f, 1.0f };
        context->ClearRenderTargetView(rtv.Get(), color);
        ResolveTexture2D(context, dst.Get(), src.Get());
    });

    TEST_RUN("GenerateMips optional", {
        if (!IsAutoGenerateMipsSupported(device, DXGI_FORMAT_R8G8B8A8_UNORM)) {
            TestUtil::Log("Skipping mipmaps: format autogen unsupported");
            return;
        }
        auto texture = CreateTexture(
            device,
            16,
            16,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
            nullptr,
            1,
            D3D11_RESOURCE_MISC_GENERATE_MIPS,
            5);
        if (!CanGenerateMipsForTexture2D(device, texture.Get())) {
            throw std::runtime_error("expected texture to support mip generation");
        }
        auto srv = CreateMipGenerationTexture2DSRV(device, texture.Get());
        GenerateMips(context, srv.Get());
    });

    TEST_RUN("Invalid argument checks", {
        ExpectThrows("CopyResource null", [&] { CopyResource(nullptr, nullptr, nullptr); });
        ExpectThrows("CalcD3D11Subresource mip", [&] { (void)CalcD3D11Subresource(1, 0, 1); });
        ExpectThrows("CopyTexture2DRegion zero", [&] {
            auto t = CreateTexture(device, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
            D3D11Texture2DRegion r{};
            CopyTexture2DRegion(context, t.Get(), t.Get(), r);
        });
        ExpectThrows("CopyBufferRegion zero", [&] {
            auto b = CreateBuffer(device, 16);
            CopyBufferRegion(context, b.Get(), b.Get(), {});
        });
        ExpectThrows("ResolveSubresource unknown", [&] {
            ResolveSubresource(context, nullptr, 0, nullptr, 0, DXGI_FORMAT_UNKNOWN);
        });
        ExpectThrows("GenerateMips null", [&] { GenerateMips(context, nullptr); });
    });

    return TestUtil::Result("CopyResolveMipmap");
}
