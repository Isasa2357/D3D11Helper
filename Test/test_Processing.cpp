#include "TestCommon.hpp"
#include <D3D11Helper/D3D11Processing/D3D11Processing.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <vector>
#include <utility>

using namespace D3D11CoreLib;
using namespace D3D11CoreLib::Processing;

namespace {

std::filesystem::path ProcessingShaderDir() {
    const auto runtimeDir = std::filesystem::current_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(runtimeDir / "ConvertRgbToRgb.hlsl")) return runtimeDir;
#ifdef D3D11HELPER_TEST_SOURCE_DIR
    const auto sourceDir = std::filesystem::u8path(D3D11HELPER_TEST_SOURCE_DIR).parent_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(sourceDir / "ConvertRgbToRgb.hlsl")) return sourceDir;
#endif
    return runtimeDir;
}

struct Fixture {
    std::shared_ptr<D3D11Core> core;
    D3D11ProcessingContext processing;
    explicit Fixture(std::shared_ptr<D3D11Core> c) : core(std::move(c)) {
        processing.Initialize(*core, ProcessingShaderDir());
    }
};

void RequireBytesEqual(const std::vector<uint8_t>& actual, const std::vector<uint8_t>& expected, const char* label) {
    if (actual.size() != expected.size()) {
        std::ostringstream os; os << label << ": size mismatch actual=" << actual.size() << " expected=" << expected.size();
        ::Test::Fail(__FILE__, __LINE__, os.str());
    }
    for (size_t i = 0; i < expected.size(); ++i) {
        if (actual[i] != expected[i]) {
            std::ostringstream os; os << label << ": byte mismatch at index " << i << " actual=" << int(actual[i]) << " expected=" << int(expected[i]);
            ::Test::Fail(__FILE__, __LINE__, os.str());
        }
    }
}

void RequireByteNear(uint8_t actual, uint8_t expected, uint8_t tolerance, const char* label, size_t index) {
    const int diff = actual > expected ? actual - expected : expected - actual;
    if (diff > tolerance) {
        std::ostringstream os; os << label << ": byte mismatch at index " << index << " actual=" << int(actual) << " expected=" << int(expected);
        ::Test::Fail(__FILE__, __LINE__, os.str());
    }
}

std::vector<uint8_t> ReadbackRgba8(D3D11Core& core, D3D11Resource& tex) {
    const auto desc = tex.GetTexture2DDesc();
    D3D11_TEXTURE2D_DESC sd = desc;
    sd.BindFlags = 0;
    sd.MiscFlags = 0;
    sd.Usage = D3D11_USAGE_STAGING;
    sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    D3D11CORE_THROW_IF_FAILED(core.GetDevice()->CreateTexture2D(&sd, nullptr, &staging));
    core.GetImmediateContext()->CopyResource(staging.Get(), tex.Get());
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    D3D11CORE_THROW_IF_FAILED(core.GetImmediateContext()->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));
    std::vector<uint8_t> out(static_cast<size_t>(desc.Width) * desc.Height * 4u);
    for (UINT y = 0; y < desc.Height; ++y) {
        std::memcpy(out.data() + static_cast<size_t>(y) * desc.Width * 4u,
                    static_cast<const uint8_t*>(mapped.pData) + static_cast<size_t>(y) * mapped.RowPitch,
                    static_cast<size_t>(desc.Width) * 4u);
    }
    core.GetImmediateContext()->Unmap(staging.Get(), 0);
    return out;
}

struct Nv12Data { std::vector<uint8_t> y; std::vector<uint8_t> uv; };
Nv12Data ReadbackNv12(D3D11Core& core, D3D11Resource& tex) {
    const auto desc = tex.GetTexture2DDesc();
    D3D11_TEXTURE2D_DESC sd = desc; sd.BindFlags = 0; sd.MiscFlags = 0; sd.Usage = D3D11_USAGE_STAGING; sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    D3D11CORE_THROW_IF_FAILED(core.GetDevice()->CreateTexture2D(&sd, nullptr, &staging));
    core.GetImmediateContext()->CopyResource(staging.Get(), tex.Get());
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    D3D11CORE_THROW_IF_FAILED(core.GetImmediateContext()->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));
    Nv12Data out; out.y.resize(static_cast<size_t>(desc.Width) * desc.Height); out.uv.resize(static_cast<size_t>(desc.Width) * (desc.Height / 2));
    const auto* base = static_cast<const uint8_t*>(mapped.pData);
    for (UINT y = 0; y < desc.Height; ++y) std::memcpy(out.y.data() + static_cast<size_t>(y) * desc.Width, base + static_cast<size_t>(y) * mapped.RowPitch, desc.Width);
    const auto* uvBase = base + static_cast<size_t>(mapped.RowPitch) * desc.Height;
    for (UINT y = 0; y < desc.Height / 2; ++y) std::memcpy(out.uv.data() + static_cast<size_t>(y) * desc.Width, uvBase + static_cast<size_t>(y) * mapped.RowPitch, desc.Width);
    core.GetImmediateContext()->Unmap(staging.Get(), 0);
    return out;
}

D3D11Resource CreatePlanarTexture(D3D11Core& core, UINT w, UINT h, DXGI_FORMAT fmt, const void* data, UINT rowPitch, UINT bindFlags) {
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = w; desc.Height = h; desc.MipLevels = 1; desc.ArraySize = 1; desc.Format = fmt;
    desc.SampleDesc.Count = 1; desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = bindFlags;
    D3D11_SUBRESOURCE_DATA init = {}; init.pSysMem = data; init.SysMemPitch = rowPitch;
    ComPtr<ID3D11Texture2D> tex;
    D3D11CORE_THROW_IF_FAILED(core.GetDevice()->CreateTexture2D(&desc, &init, &tex));
    return D3D11Resource(std::move(tex));
}

D3D11Resource CreateNv12Texture(D3D11Core& core, UINT w, UINT h, uint8_t yValue = 128, uint8_t uvValue = 128, UINT bindFlags = D3D11_BIND_SHADER_RESOURCE) {
    std::vector<uint8_t> data(static_cast<size_t>(w) * h + static_cast<size_t>(w) * (h / 2));
    std::fill(data.begin(), data.begin() + static_cast<size_t>(w) * h, yValue);
    std::fill(data.begin() + static_cast<size_t>(w) * h, data.end(), uvValue);
    return CreatePlanarTexture(core, w, h, DXGI_FORMAT_NV12, data.data(), w, bindFlags);
}

D3D11Resource CreateP010Texture(D3D11Core& core, UINT w, UINT h, uint16_t yValue = 32768, uint16_t uvValue = 32768, UINT bindFlags = D3D11_BIND_SHADER_RESOURCE) {
    std::vector<uint16_t> data(static_cast<size_t>(w) * h + static_cast<size_t>(w) * (h / 2));
    std::fill(data.begin(), data.begin() + static_cast<size_t>(w) * h, yValue);
    std::fill(data.begin() + static_cast<size_t>(w) * h, data.end(), uvValue);
    return CreatePlanarTexture(core, w, h, DXGI_FORMAT_P010, data.data(), w * sizeof(uint16_t), bindFlags);
}

std::vector<uint8_t> MakePointResizeExpected2x2To4x4(const std::vector<uint8_t>& src) {
    std::vector<uint8_t> expected(4u * 4u * 4u);
    for (UINT y = 0; y < 4; ++y) for (UINT x = 0; x < 4; ++x) {
        const UINT sx = x / 2u; const UINT sy = y / 2u;
        std::memcpy(expected.data() + static_cast<size_t>(y * 4u + x) * 4u, src.data() + static_cast<size_t>(sy * 2u + sx) * 4u, 4u);
    }
    return expected;
}

} // namespace

TEST(Processing, TypesAndShaderCompile) {
    REQUIRE_CORE(core);
    Fixture fx(core);
    CHECK(IsRgbaLikeFormat(DXGI_FORMAT_R8G8B8A8_UNORM));
    CHECK(IsRgbaLikeFormat(DXGI_FORMAT_R16G16B16A16_FLOAT));
    CHECK(IsYuv420Format(DXGI_FORMAT_NV12));
    CHECK(IsYuv420Format(DXGI_FORMAT_P010));

    D3D11ProcessingShaderCache cache;
    cache.Initialize(fx.processing);
    CHECK(!cache.GetComputeShader("ConvertRgbToRgb.hlsl").Empty());
    CHECK(!cache.GetComputeShader("ConvertYuv420ToRgb.hlsl").Empty());
    CHECK(!cache.GetComputeShader("ConvertRgbToYuv420.hlsl").Empty());
    CHECK(!cache.GetComputeShader("ResizeRgba.hlsl").Empty());
    CHECK(!cache.GetComputeShader("RemapRgba.hlsl").Empty());
    CHECK(!cache.GetComputeShader("CompositeRgba.hlsl").Empty());
    CHECK(!cache.GetComputeShader("FusedYuv420ToRgbResize.hlsl").Empty());
}

TEST(Processing, RgbaCopyAndResizeReadback) {
    REQUIRE_CORE(core);
    Fixture fx(core);
    if (!fx.processing.SupportsRgba8Uav()) { std::cout << "R8G8B8A8 UAV not supported; skipping\n"; return; }
    const std::vector<uint8_t> srcPixels = { 10,20,30,255, 40,50,60,255, 70,80,90,255, 100,110,120,255 };
    auto src = CreateTexture2DFromRGBA(*core, srcPixels.data(), 2, 2, D3D11_BIND_SHADER_RESOURCE);
    D3D11FormatConverter conv; conv.Initialize(fx.processing);
    auto copy = conv.CreateOutputTexture(*core, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM);
    FormatConvertDesc cd = {}; cd.srcFormat = DXGI_FORMAT_R8G8B8A8_UNORM; cd.dstFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    conv.DispatchConvert(core->GetImmediateContext(), src, copy, cd);
    RequireBytesEqual(ReadbackRgba8(*core, copy), srcPixels, "rgba copy");

    D3D11Resizer resizer; resizer.Initialize(fx.processing);
    auto dst = resizer.CreateOutputTexture(*core, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM);
    ResizeDesc rd = {}; rd.filter = ProcessingFilter::Point;
    resizer.DispatchResize(core->GetImmediateContext(), src, dst, rd);
    RequireBytesEqual(ReadbackRgba8(*core, dst), MakePointResizeExpected2x2To4x4(srcPixels), "point resize");
}

TEST(Processing, Nv12AndFusedReadback) {
    REQUIRE_CORE(core);
    Fixture fx(core);
    if (!fx.processing.SupportsNv12Srv() || !fx.processing.SupportsRgba8Uav()) { std::cout << "NV12 SRV or RGBA UAV not supported; skipping\n"; return; }
    auto nv12 = CreateNv12Texture(*core, 4, 4, 128, 128);
    D3D11FormatConverter conv; conv.Initialize(fx.processing);
    auto rgba = conv.CreateOutputTexture(*core, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM);
    FormatConvertDesc desc = {}; desc.srcFormat = DXGI_FORMAT_NV12; desc.dstFormat = DXGI_FORMAT_R8G8B8A8_UNORM; desc.color.srcRange = ProcessingColorRange::Full;
    conv.DispatchConvert(core->GetImmediateContext(), nv12, rgba, desc);
    const auto got = ReadbackRgba8(*core, rgba);
    for (size_t i = 0; i < got.size(); i += 4) {
        RequireByteNear(got[i + 0], 128, 3, "nv12 gray r", i + 0);
        RequireByteNear(got[i + 1], 128, 3, "nv12 gray g", i + 1);
        RequireByteNear(got[i + 2], 128, 3, "nv12 gray b", i + 2);
        CHECK_EQ(got[i + 3], 255);
    }

    D3D11FusedProcessor fused; fused.Initialize(fx.processing);
    auto fusedSmall = fused.CreateOutputTexture(*core, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM);
    FusedConvertResizeDesc fd = {}; fd.srcFormat = DXGI_FORMAT_NV12; fd.dstFormat = DXGI_FORMAT_R8G8B8A8_UNORM; fd.filter = ProcessingFilter::Point;
    fused.DispatchConvertResize(core->GetImmediateContext(), nv12, fusedSmall, fd);
    CHECK_EQ(ReadbackRgba8(*core, fusedSmall).size(), static_cast<size_t>(2u * 2u * 4u));
}

TEST(Processing, RgbaToNv12Readback) {
    REQUIRE_CORE(core);
    Fixture fx(core);
    if (!fx.processing.SupportsNv12Uav() || !fx.processing.SupportsRgba8Uav()) { std::cout << "NV12 UAV not supported; skipping\n"; return; }
    std::vector<uint8_t> white(4u * 4u * 4u, 255);
    auto rgba = CreateTexture2DFromRGBA(*core, white.data(), 4, 4, D3D11_BIND_SHADER_RESOURCE);
    D3D11FormatConverter conv; conv.Initialize(fx.processing);
    auto nv12 = conv.CreateOutputTexture(*core, 4, 4, DXGI_FORMAT_NV12);
    FormatConvertDesc desc = {}; desc.srcFormat = DXGI_FORMAT_R8G8B8A8_UNORM; desc.dstFormat = DXGI_FORMAT_NV12; desc.color.dstRange = ProcessingColorRange::Full;
    conv.DispatchConvert(core->GetImmediateContext(), rgba, nv12, desc);
    const auto got = ReadbackNv12(*core, nv12);
    for (uint8_t v : got.y) RequireByteNear(v, 255, 2, "rgba->nv12 y", 0);
    for (uint8_t v : got.uv) RequireByteNear(v, 128, 2, "rgba->nv12 uv", 0);
}

TEST(Processing, RemapCompositeAndP010) {
    REQUIRE_CORE(core);
    Fixture fx(core);
    if (!fx.processing.SupportsRgba8Uav()) { std::cout << "RGBA UAV not supported; skipping\n"; return; }
    const std::vector<uint8_t> srcPixels = { 255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,255,255 };
    auto src = CreateTexture2DFromRGBA(*core, srcPixels.data(), 2, 2, D3D11_BIND_SHADER_RESOURCE);
    const std::vector<float> mapPixels = { 1,0, 0,0, 1,1, 0,1 };
    auto map = CreateTexture2DFromMemory(*core, mapPixels.data(), 2, 2, DXGI_FORMAT_R32G32_FLOAT, 2 * sizeof(float) * 2, D3D11_BIND_SHADER_RESOURCE);
    D3D11Remapper remapper; remapper.Initialize(fx.processing); auto remapped = remapper.CreateOutputTexture(*core, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM);
    RemapDesc rdesc = {}; rdesc.filter = ProcessingFilter::Point;
    remapper.DispatchRemap(core->GetImmediateContext(), src, map, remapped, rdesc);
    const std::vector<uint8_t> flipExpected = { 0,255,0,255, 255,0,0,255, 255,255,255,255, 0,0,255,255 };
    RequireBytesEqual(ReadbackRgba8(*core, remapped), flipExpected, "remap flip");

    std::vector<uint8_t> base(4u * 4u, 0); for (size_t i=0;i<base.size();i+=4){ base[i]=100; base[i+3]=255; }
    std::vector<uint8_t> overlay(4u * 4u, 0); for (size_t i=0;i<overlay.size();i+=4){ overlay[i+2]=200; overlay[i+3]=128; }
    auto baseTex = CreateTexture2DFromRGBA(*core, base.data(), 2, 2, D3D11_BIND_SHADER_RESOURCE);
    auto overlayTex = CreateTexture2DFromRGBA(*core, overlay.data(), 2, 2, D3D11_BIND_SHADER_RESOURCE);
    D3D11Compositor compositor; compositor.Initialize(fx.processing); auto comp = compositor.CreateOutputTexture(*core, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM);
    CompositeDesc cdesc = {}; cdesc.blendMode = CompositeBlendMode::AlphaBlend;
    compositor.DispatchComposite(core->GetImmediateContext(), baseTex, overlayTex, comp, cdesc);
    auto compGot = ReadbackRgba8(*core, comp);
    for (size_t i=0;i<compGot.size();i+=4){ RequireByteNear(compGot[i], 50, 1, "comp r", i); RequireByteNear(compGot[i+2], 100, 1, "comp b", i+2); CHECK_EQ(compGot[i+3], 255); }

    if (fx.processing.SupportsP010Srv()) {
        auto p010 = CreateP010Texture(*core, 4, 4, 32768, 32768);
        D3D11FormatConverter conv; conv.Initialize(fx.processing); auto p010Out = conv.CreateOutputTexture(*core, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM);
        FormatConvertDesc pd = {}; pd.srcFormat = DXGI_FORMAT_P010; pd.dstFormat = DXGI_FORMAT_R8G8B8A8_UNORM; pd.color.srcRange = ProcessingColorRange::Full;
        conv.DispatchConvert(core->GetImmediateContext(), p010, p010Out, pd);
        CHECK_EQ(ReadbackRgba8(*core, p010Out).size(), 4u * 4u * 4u);
    }
}
