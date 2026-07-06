#include "TestCommon.hpp"

#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
#include <D3D11Helper/D3D11Processing/D3D11Processing.hpp>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <utility>
#include <vector>

using namespace D3D11CoreLib;
using namespace D3D11CoreLib::Processing;

namespace {

std::filesystem::path ProcessingShaderDir() {
    const auto runtimeDir = std::filesystem::current_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(runtimeDir / "MaskApplyRgba.hlsl")) return runtimeDir;
#ifdef D3D11HELPER_TEST_SOURCE_DIR
    const auto sourceDir = std::filesystem::u8path(D3D11HELPER_TEST_SOURCE_DIR).parent_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(sourceDir / "MaskApplyRgba.hlsl")) return sourceDir;
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

void RequireByteNear(uint8_t actual, uint8_t expected, uint8_t tolerance, const char* label, size_t index) {
    const int diff = actual > expected ? actual - expected : expected - actual;
    if (diff > tolerance) {
        std::ostringstream os;
        os << label << ": byte mismatch at index " << index << " actual=" << int(actual) << " expected=" << int(expected);
        ::Test::Fail(__FILE__, __LINE__, os.str());
    }
}

} // namespace

TEST(ProcessingMask, ShaderCompile) {
    REQUIRE_CORE(core);
    Fixture fx(core);
    D3D11ProcessingShaderCache cache;
    cache.Initialize(fx.processing);
    CHECK(!cache.GetComputeShader("MaskApplyRgba.hlsl").Empty());
    CHECK(!cache.GetComputeShader("MaskBlendRgba.hlsl").Empty());
    CHECK(!cache.GetComputeShader("MaskCombineRgba.hlsl").Empty());
    CHECK(!cache.GetComputeShader("MaskInvertRgba.hlsl").Empty());
}

TEST(ProcessingMask, ApplyAlphaAndMultiplyReadback) {
    REQUIRE_CORE(core);
    Fixture fx(core);
    if (!fx.processing.SupportsRgba8Uav()) { std::cout << "R8G8B8A8 UAV not supported; skipping\n"; return; }

    const std::vector<uint8_t> srcPixels = {
        100, 50, 25, 200,  100, 50, 25, 200,
        100, 50, 25, 200,  100, 50, 25, 200,
    };
    const std::vector<uint8_t> maskPixels = {
        0, 0, 0, 0,       0, 0, 0, 128,
        0, 0, 0, 192,     0, 0, 0, 255,
    };

    auto src = CreateTexture2DFromRGBA(*core, srcPixels.data(), 2, 2, D3D11_BIND_SHADER_RESOURCE);
    auto mask = CreateTexture2DFromRGBA(*core, maskPixels.data(), 2, 2, D3D11_BIND_SHADER_RESOURCE);
    D3D11MaskProcessor proc;
    proc.Initialize(fx.processing);

    auto alphaDst = proc.CreateOutputTexture(*core, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM);
    MaskApplyDesc ad = {};
    ad.mode = MaskApplyMode::ApplyAlpha;
    ad.channel = MaskChannel::Alpha;
    proc.DispatchApplyMask(core->GetImmediateContext(), src, mask, alphaDst, ad);
    auto alphaGot = ReadbackRgba8(*core, alphaDst);
    const uint8_t expectedAlpha[4] = { 0, 100, 151, 200 };
    for (size_t px = 0; px < 4; ++px) {
        const size_t i = px * 4;
        CHECK_EQ(alphaGot[i + 0], 100);
        CHECK_EQ(alphaGot[i + 1], 50);
        CHECK_EQ(alphaGot[i + 2], 25);
        RequireByteNear(alphaGot[i + 3], expectedAlpha[px], 2, "apply alpha", i + 3);
    }

    auto rgbDst = proc.CreateOutputTexture(*core, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM);
    MaskApplyDesc md = {};
    md.mode = MaskApplyMode::MultiplyRgb;
    md.channel = MaskChannel::Alpha;
    proc.DispatchApplyMask(core->GetImmediateContext(), src, mask, rgbDst, md);
    auto rgbGot = ReadbackRgba8(*core, rgbDst);
    RequireByteNear(rgbGot[0], 0, 1, "multiply rgb first r", 0);
    RequireByteNear(rgbGot[4], 50, 2, "multiply rgb second r", 4);
    RequireByteNear(rgbGot[12], 100, 1, "multiply rgb fourth r", 12);
    CHECK_EQ(rgbGot[3], 200);
    CHECK_EQ(rgbGot[7], 200);
    CHECK_EQ(rgbGot[15], 200);
}

TEST(ProcessingMask, BlendCombineAndInvertReadback) {
    REQUIRE_CORE(core);
    Fixture fx(core);
    if (!fx.processing.SupportsRgba8Uav()) { std::cout << "R8G8B8A8 UAV not supported; skipping\n"; return; }

    const std::vector<uint8_t> basePixels(4u * 4u, 0);
    const std::vector<uint8_t> overlayPixels(4u * 4u, 255);
    const std::vector<uint8_t> maskPixels = {
        0, 0, 0, 0,       0, 0, 0, 128,
        0, 0, 0, 255,     0, 0, 0, 255,
    };
    const std::vector<uint8_t> maskBPixels = {
        0, 0, 0, 255,     0, 0, 0, 128,
        0, 0, 0, 0,       0, 0, 0, 255,
    };

    auto base = CreateTexture2DFromRGBA(*core, basePixels.data(), 2, 2, D3D11_BIND_SHADER_RESOURCE);
    auto overlay = CreateTexture2DFromRGBA(*core, overlayPixels.data(), 2, 2, D3D11_BIND_SHADER_RESOURCE);
    auto maskA = CreateTexture2DFromRGBA(*core, maskPixels.data(), 2, 2, D3D11_BIND_SHADER_RESOURCE);
    auto maskB = CreateTexture2DFromRGBA(*core, maskBPixels.data(), 2, 2, D3D11_BIND_SHADER_RESOURCE);

    D3D11MaskProcessor proc;
    proc.Initialize(fx.processing);

    auto blendDst = proc.CreateOutputTexture(*core, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM);
    MaskBlendDesc bd = {};
    bd.channel = MaskChannel::Alpha;
    proc.DispatchBlendByMask(core->GetImmediateContext(), base, overlay, maskA, blendDst, bd);
    auto blendGot = ReadbackRgba8(*core, blendDst);
    RequireByteNear(blendGot[0], 0, 1, "blend first", 0);
    RequireByteNear(blendGot[4], 128, 2, "blend second", 4);
    RequireByteNear(blendGot[8], 255, 1, "blend third", 8);

    auto combineDst = proc.CreateOutputTexture(*core, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM);
    MaskCombineDesc cd = {};
    cd.mode = MaskCombineMode::Max;
    cd.channelA = MaskChannel::Alpha;
    cd.channelB = MaskChannel::Alpha;
    proc.DispatchCombineMasks(core->GetImmediateContext(), maskA, maskB, combineDst, cd);
    auto combineGot = ReadbackRgba8(*core, combineDst);
    RequireByteNear(combineGot[0], 255, 1, "combine first", 0);
    RequireByteNear(combineGot[4], 128, 2, "combine second", 4);
    RequireByteNear(combineGot[8], 255, 1, "combine third", 8);

    auto invertDst = proc.CreateOutputTexture(*core, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM);
    MaskInvertDesc id = {};
    id.channel = MaskChannel::Alpha;
    proc.DispatchInvertMask(core->GetImmediateContext(), maskA, invertDst, id);
    auto invertGot = ReadbackRgba8(*core, invertDst);
    RequireByteNear(invertGot[0], 255, 1, "invert first", 0);
    RequireByteNear(invertGot[4], 127, 2, "invert second", 4);
    RequireByteNear(invertGot[8], 0, 1, "invert third", 8);
}
