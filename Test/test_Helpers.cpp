#include "TestCommon.hpp"

#include <array>
#include <cstdint>
#include <vector>

using namespace D3D11CoreLib;

TEST(Helpers, FullDescTextureViewsAndSampler) {
    REQUIRE_CORE(core);

    D3D11Resource srvTex = CreateTexture2D(
        *core, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM,
        D3D11_BIND_SHADER_RESOURCE);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    auto srv = CreateSrv(*core, srvTex.Get(), srvDesc);
    CHECK(srv != nullptr);

    D3D11Resource uavTex = CreateTexture2D(
        *core, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM,
        D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE);

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Texture2D.MipSlice = 0;

    auto uav = CreateUav(*core, uavTex.Get(), uavDesc);
    CHECK(uav != nullptr);

    D3D11Resource rt = CreateTexture2D(
        *core, 16, 16, DXGI_FORMAT_R8G8B8A8_UNORM,
        D3D11_BIND_RENDER_TARGET);

    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

    auto rtv = CreateRtv(*core, rt.Get(), rtvDesc);
    CHECK(rtv != nullptr);

    D3D11Resource ds = CreateTexture2D(
        *core, 16, 16, DXGI_FORMAT_D32_FLOAT,
        D3D11_BIND_DEPTH_STENCIL);

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

    auto dsv = CreateDsv(*core, ds.Get(), dsvDesc);
    CHECK(dsv != nullptr);

    D3D11_SAMPLER_DESC linear = MakeLinearClampSamplerDesc();
    CHECK(linear.Filter == D3D11_FILTER_MIN_MAG_MIP_LINEAR);
    CHECK(linear.AddressU == D3D11_TEXTURE_ADDRESS_CLAMP);
    auto sampler = CreateSampler(*core, linear);
    CHECK(sampler != nullptr);
}

TEST(Helpers, StructuredAndTypedBufferViews) {
    REQUIRE_CORE(core);

    std::array<float, 16> values = {};
    D3D11Resource structured = CreateStructuredBuffer(
        *core,
        static_cast<UINT>(values.size()),
        sizeof(float),
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
        values.data());

    auto structuredSrv = CreateBufferSrv(*core, structured, 0, static_cast<UINT>(values.size()));
    auto structuredUav = CreateBufferUav(*core, structured, 0, static_cast<UINT>(values.size()));
    CHECK(structuredSrv != nullptr);
    CHECK(structuredUav != nullptr);
    CHECK_THROWS(CreateBufferSrv(*core, structured, 0, 4, DXGI_FORMAT_R32_FLOAT));

    D3D11Resource typed = CreateBuffer(
        *core,
        static_cast<UINT>(values.size() * sizeof(float)),
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS,
        0, 0,
        values.data());

    auto typedSrv = CreateBufferSrv(*core, typed, 0, static_cast<UINT>(values.size()), DXGI_FORMAT_R32_FLOAT);
    auto typedUav = CreateBufferUav(*core, typed, 0, static_cast<UINT>(values.size()), DXGI_FORMAT_R32_FLOAT);
    CHECK(typedSrv != nullptr);
    CHECK(typedUav != nullptr);
    CHECK_THROWS(CreateBufferSrv(*core, typed, 0, static_cast<UINT>(values.size())));
}

TEST(Helpers, TextureSubresourceCreateAndUpdate) {
    REQUIRE_CORE(core);

    const UINT width = 4;
    const UINT height = 4;
    std::vector<uint8_t> mip0(width * height * 4, 64);
    std::vector<uint8_t> mip1((width / 2) * (height / 2) * 4, 128);

    D3D11TextureSubresourceData init[2] = {};
    init[0].data = mip0.data();
    init[0].rowPitch = width * 4;
    init[1].data = mip1.data();
    init[1].rowPitch = (width / 2) * 4;

    D3D11Resource tex = CreateTexture2DFromSubresources(
        *core,
        width,
        height,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        init,
        2,
        D3D11_BIND_SHADER_RESOURCE,
        D3D11_USAGE_DEFAULT,
        0,
        1,
        2);

    CHECK(tex.Get() != nullptr);
    auto srv = CreateTexture2DSrv(*core, tex);
    CHECK(srv != nullptr);

    std::vector<uint8_t> update0(width * height * 4, 32);
    std::vector<uint8_t> update1((width / 2) * (height / 2) * 4, 96);
    D3D11TextureSubresourceData upd[2] = {};
    upd[0].data = update0.data();
    upd[0].rowPitch = width * 4;
    upd[1].data = update1.data();
    upd[1].rowPitch = (width / 2) * 4;

    CHECK_NOTHROW(UpdateTextureSubresources(*core, tex, upd, 0, 2));
}

TEST(Helpers, SingleSubresourceGuards) {
    REQUIRE_CORE(core);

    std::vector<uint8_t> rgba(4 * 4 * 4, 255);
    CHECK_THROWS(CreateTexture2DFromMemory(
        *core,
        rgba.data(),
        4,
        4,
        DXGI_FORMAT_NV12));

    D3D11Resource arrayTex = CreateTexture2D(
        *core,
        4,
        4,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        D3D11_BIND_SHADER_RESOURCE,
        D3D11_USAGE_DEFAULT,
        0,
        2,
        1);

    CHECK_THROWS(UpdateTexture2D(
        *core,
        arrayTex,
        rgba.data(),
        4,
        4,
        DXGI_FORMAT_R8G8B8A8_UNORM));
}
