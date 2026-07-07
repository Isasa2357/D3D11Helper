//
// test_view_state.cpp - D3D11 advanced view / state helper tests
//
#include "TestUtil.hpp"

#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11View.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11State.hpp>

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>

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

ComPtr<ID3D11Texture2D> CreateTexture2D(
    ID3D11Device* device,
    UINT width,
    UINT height,
    DXGI_FORMAT format,
    UINT bindFlags,
    UINT arraySize = 1,
    UINT mipLevels = 1,
    UINT miscFlags = 0,
    UINT sampleCount = 1) {
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = mipLevels;
    desc.ArraySize = arraySize;
    desc.Format = format;
    desc.SampleDesc.Count = sampleCount;
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
    auto core = TestUtil::MakeCore();
    ID3D11Device* device = core->GetDevice();

    TEST_RUN("Texture2D array views", {
        auto colorTexture = CreateTexture2D(
            device,
            8,
            8,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
            2);

        D3D11Texture2DArrayViewDesc desc{};
        desc.firstArraySlice = 0;
        desc.arraySize = 2;
        auto srv = CreateTexture2DArraySrv(device, colorTexture.Get(), desc);
        auto rtv = CreateTexture2DArrayRtv(device, colorTexture.Get(), desc);

        auto uavTexture = CreateTexture2D(
            device,
            8,
            8,
            DXGI_FORMAT_R32_UINT,
            D3D11_BIND_UNORDERED_ACCESS,
            2);
        auto uav = CreateTexture2DArrayUav(device, uavTexture.Get(), desc);
        if (!srv || !rtv || !uav) throw std::runtime_error("Texture2D array view creation returned null");
    });

    TEST_RUN("Depth Texture2D array DSV/SRV", {
        auto depthArray = CreateTexture2D(
            device,
            8,
            8,
            DXGI_FORMAT_R24G8_TYPELESS,
            D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE,
            2);

        D3D11Texture2DArrayViewDesc desc{};
        desc.arraySize = 2;
        auto dsv = CreateTexture2DArrayDsv(device, depthArray.Get(), desc);
        desc.format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        auto srv = CreateTexture2DArraySrv(device, depthArray.Get(), desc);
        if (!dsv || !srv) throw std::runtime_error("depth array view creation returned null");
    });


    TEST_RUN("Texture cube SRV", {
        auto texture = CreateTexture2D(
            device,
            4,
            4,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            D3D11_BIND_SHADER_RESOURCE,
            6,
            1,
            D3D11_RESOURCE_MISC_TEXTURECUBE);
        auto srv = CreateTextureCubeSrv(device, texture.Get());
        if (!srv) throw std::runtime_error("cube SRV creation returned null");
    });

    TEST_RUN("Texture cube array SRV", {
        auto texture = CreateTexture2D(
            device,
            4,
            4,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            D3D11_BIND_SHADER_RESOURCE,
            12,
            1,
            D3D11_RESOURCE_MISC_TEXTURECUBE);

        D3D11TextureCubeArrayViewDesc desc{};
        desc.cubeCount = 2;
        auto srv = CreateTextureCubeArraySrv(device, texture.Get(), desc);
        if (!srv) throw std::runtime_error("cube array SRV creation returned null");

        ExpectThrows("CreateTextureCubeSrv cube array", [&] {
            (void)CreateTextureCubeSrv(device, texture.Get());
        });
    });


    TEST_RUN("Depth DSV and SRV", {
        auto texture = CreateTexture2D(
            device,
            8,
            8,
            DXGI_FORMAT_R24G8_TYPELESS,
            D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE);
        auto dsv = CreateDepthTexture2DDsv(device, texture.Get());
        auto srv = CreateDepthTexture2DSrv(device, texture.Get());
        if (!dsv || !srv) throw std::runtime_error("depth view creation returned null");
        if (GetDepthStencilViewFormat(DXGI_FORMAT_R24G8_TYPELESS) != DXGI_FORMAT_D24_UNORM_S8_UINT) {
            throw std::runtime_error("unexpected DSV depth mapping");
        }
        if (GetDepthShaderResourceViewFormat(DXGI_FORMAT_R24G8_TYPELESS) != DXGI_FORMAT_R24_UNORM_X8_TYPELESS) {
            throw std::runtime_error("unexpected SRV depth mapping");
        }
    });

    TEST_RUN("Typed buffer views", {
        auto buffer = CreateBuffer(device, 64, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
        auto srv = CreateTypedBufferSrv(device, buffer.Get(), DXGI_FORMAT_R32_UINT, 0, 16);
        auto uav = CreateTypedBufferUav(device, buffer.Get(), DXGI_FORMAT_R32_UINT, 0, 16);
        if (!srv || !uav) throw std::runtime_error("typed buffer view creation returned null");
    });

    TEST_RUN("Structured buffer views", {
        auto buffer = CreateBuffer(
            device,
            16 * 4,
            D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
            D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
            16);
        auto srv = CreateStructuredBufferSrv(device, buffer.Get(), 0, 4);
        auto uav = CreateStructuredBufferUav(device, buffer.Get(), 0, 4);
        if (!srv || !uav) throw std::runtime_error("structured buffer view creation returned null");
    });

    TEST_RUN("Raw buffer views", {
        auto buffer = CreateBuffer(
            device,
            64,
            D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
            D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS);
        auto srv = CreateRawBufferSrv(device, buffer.Get(), 0, 16);
        auto uav = CreateRawBufferUav(device, buffer.Get(), 0, 16);
        if (!srv || !uav) throw std::runtime_error("raw buffer view creation returned null");
    });

    TEST_RUN("State preset creation", {
        auto samplerPoint = CreateSamplerState(device, StatePresets::SamplerPointClamp());
        auto samplerLinear = CreateSamplerState(device, StatePresets::SamplerLinearWrap());
        auto samplerAniso = CreateSamplerState(device, StatePresets::SamplerAnisotropicWrap());
        auto samplerCompare = CreateSamplerState(device, StatePresets::SamplerComparisonLinearClamp());
        auto rasterBack = CreateRasterizerState(device, StatePresets::RasterizerCullBack());
        auto rasterNone = CreateRasterizerState(device, StatePresets::RasterizerCullNone());
        auto rasterWire = CreateRasterizerState(device, StatePresets::RasterizerWireframe());
        auto blendOpaque = CreateBlendState(device, StatePresets::BlendOpaque());
        auto blendAlpha = CreateBlendState(device, StatePresets::BlendAlpha());
        auto blendPremul = CreateBlendState(device, StatePresets::BlendPremultipliedAlpha());
        auto blendAdd = CreateBlendState(device, StatePresets::BlendAdditive());
        auto depthDisabled = CreateDepthStencilState(device, StatePresets::DepthDisabled());
        auto depthDefault = CreateDepthStencilState(device, StatePresets::DepthDefault());
        auto depthReadOnly = CreateDepthStencilState(device, StatePresets::DepthReadOnly());
        auto depthReverse = CreateDepthStencilState(device, StatePresets::DepthReverseZ());
        if (!samplerPoint || !samplerLinear || !samplerAniso || !samplerCompare ||
            !rasterBack || !rasterNone || !rasterWire ||
            !blendOpaque || !blendAlpha || !blendPremul || !blendAdd ||
            !depthDisabled || !depthDefault || !depthReadOnly || !depthReverse) {
            throw std::runtime_error("state object creation returned null");
        }
    });

    TEST_RUN("Invalid argument checks", {
        ExpectThrows("CreateSamplerState null", [&] {
            (void)CreateSamplerState(nullptr, StatePresets::SamplerPointClamp());
        });
        ExpectThrows("CreateTexture2DArraySrv null", [&] {
            (void)CreateTexture2DArraySrv(nullptr, nullptr);
        });
        ExpectThrows("CreateTexture2DArraySrv range", [&] {
            auto texture = CreateTexture2D(device, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, 1);
            D3D11Texture2DArrayViewDesc desc{};
            desc.firstArraySlice = 1;
            desc.arraySize = 1;
            (void)CreateTexture2DArraySrv(device, texture.Get(), desc);
        });
        ExpectThrows("CreateRawBufferSrv missing flag", [&] {
            auto buffer = CreateBuffer(device, 16, D3D11_BIND_SHADER_RESOURCE);
            (void)CreateRawBufferSrv(device, buffer.Get(), 0, 4);
        });
        ExpectThrows("CreateDepthTexture2DSrv typed color", [&] {
            auto texture = CreateTexture2D(device, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE);
            (void)CreateDepthTexture2DSrv(device, texture.Get());
        });
    });

    return TestUtil::Result("ViewState");
}
