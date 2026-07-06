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
    if (std::filesystem::exists(runtimeDir / "ThresholdRgba.hlsl")) {
        return runtimeDir;
    }

#ifdef D3D11HELPER_TEST_SOURCE_DIR
    const auto sourceDir = std::filesystem::u8path(D3D11HELPER_TEST_SOURCE_DIR).parent_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(sourceDir / "ThresholdRgba.hlsl")) {
        return sourceDir;
    }
#endif

    return runtimeDir;
}

struct Fixture {
    std::shared_ptr<D3D11Core> core;
    D3D11ProcessingContext processing;

    explicit Fixture(std::shared_ptr<D3D11Core> c)
        : core(std::move(c)) {
        processing.Initialize(*core, ProcessingShaderDir());
    }
};

std::vector<uint8_t> ReadbackRgba8(D3D11Core& core, D3D11Resource& tex) {
    const auto desc = tex.GetTexture2DDesc();

    D3D11_TEXTURE2D_DESC stagingDesc = desc;
    stagingDesc.BindFlags = 0;
    stagingDesc.MiscFlags = 0;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    ComPtr<ID3D11Texture2D> staging;
    D3D11CORE_THROW_IF_FAILED(core.GetDevice()->CreateTexture2D(&stagingDesc, nullptr, &staging));
    core.GetImmediateContext()->CopyResource(staging.Get(), tex.Get());

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    D3D11CORE_THROW_IF_FAILED(core.GetImmediateContext()->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));

    std::vector<uint8_t> out(static_cast<size_t>(desc.Width) * desc.Height * 4u);
    for (UINT y = 0; y < desc.Height; ++y) {
        std::memcpy(
            out.data() + static_cast<size_t>(y) * desc.Width * 4u,
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
        os << label << ": byte mismatch at index " << index
           << " actual=" << int(actual)
           << " expected=" << int(expected)
           << " tolerance=" << int(tolerance);
        ::Test::Fail(__FILE__, __LINE__, os.str());
    }
}

} // namespace

TEST(ProcessingThreshold, ShaderCompile) {
    REQUIRE_CORE(core);
    Fixture fx(core);

    D3D11ProcessingShaderCache cache;
    cache.Initialize(fx.processing);

    CHECK(!cache.GetComputeShader("ThresholdRgba.hlsl").Empty());
    CHECK(!cache.GetComputeShader("RangeThresholdRgba.hlsl").Empty());
    CHECK(!cache.GetComputeShader("ConfidenceHeatmapRgba.hlsl").Empty());
    CHECK(!cache.GetComputeShader("ClassColorMapRgba.hlsl").Empty());
    CHECK(!cache.GetComputeShader("MaskOverlayRgba.hlsl").Empty());
}

TEST(ProcessingThreshold, ThresholdReadback) {
    REQUIRE_CORE(core);
    Fixture fx(core);
    if (!fx.processing.SupportsRgba8Uav()) {
        std::cout << "R8G8B8A8 UAV not supported; skipping\n";
        return;
    }

    const std::vector<uint8_t> pixels = {
        0, 0, 0, 255,
        128, 128, 128, 255,
        255, 255, 255, 255,
        64, 64, 64, 255,
    };
    auto src = CreateTexture2DFromRGBA(*core, pixels.data(), 4, 1, D3D11_BIND_SHADER_RESOURCE);

    D3D11ThresholdProcessor proc;
    proc.Initialize(fx.processing);
    auto dst = proc.CreateOutputTexture(*core, 4, 1, DXGI_FORMAT_R8G8B8A8_UNORM);

    ThresholdDesc desc = {};
    desc.channel = MaskChannel::Luma;
    desc.threshold = 0.5f;
    desc.foregroundColor[0] = 255.0f / 255.0f;
    desc.foregroundColor[1] = 255.0f / 255.0f;
    desc.foregroundColor[2] = 255.0f / 255.0f;
    desc.foregroundColor[3] = 255.0f / 255.0f;
    desc.backgroundColor[0] = 0.0f;
    desc.backgroundColor[1] = 0.0f;
    desc.backgroundColor[2] = 0.0f;
    desc.backgroundColor[3] = 255.0f / 255.0f;

    proc.DispatchThreshold(core->GetImmediateContext(), src, dst, desc);

    const auto got = ReadbackRgba8(*core, dst);
    const std::vector<uint8_t> expected = {
        0, 0, 0, 255,
        255, 255, 255, 255,
        255, 255, 255, 255,
        0, 0, 0, 255,
    };

    CHECK_EQ(got.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        RequireByteNear(got[i], expected[i], 1, "threshold", i);
    }
}

TEST(ProcessingThreshold, RangeThresholdAndOverlayReadback) {
    REQUIRE_CORE(core);
    Fixture fx(core);
    if (!fx.processing.SupportsRgba8Uav()) {
        std::cout << "R8G8B8A8 UAV not supported; skipping\n";
        return;
    }

    const std::vector<uint8_t> pixels = {
        0, 0, 0, 0,
        80, 80, 80, 80,
        160, 160, 160, 160,
        255, 255, 255, 255,
    };
    auto src = CreateTexture2DFromRGBA(*core, pixels.data(), 4, 1, D3D11_BIND_SHADER_RESOURCE);

    D3D11ThresholdProcessor proc;
    proc.Initialize(fx.processing);
    auto rangeOut = proc.CreateOutputTexture(*core, 4, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
    auto overlayOut = proc.CreateOutputTexture(*core, 4, 1, DXGI_FORMAT_R8G8B8A8_UNORM);

    RangeThresholdDesc rd = {};
    rd.channel = MaskChannel::Red;
    rd.minValue = 0.25f;
    rd.maxValue = 0.75f;
    rd.foregroundColor[0] = 0.0f;
    rd.foregroundColor[1] = 1.0f;
    rd.foregroundColor[2] = 0.0f;
    rd.foregroundColor[3] = 1.0f;
    rd.backgroundColor[0] = 0.0f;
    rd.backgroundColor[1] = 0.0f;
    rd.backgroundColor[2] = 0.0f;
    rd.backgroundColor[3] = 1.0f;
    proc.DispatchRangeThreshold(core->GetImmediateContext(), src, rangeOut, rd);

    const auto rangeGot = ReadbackRgba8(*core, rangeOut);
    // Only 80 and 160 should pass.
    CHECK_EQ(rangeGot[1], 0);
    CHECK_EQ(rangeGot[5], 255);
    CHECK_EQ(rangeGot[9], 255);
    CHECK_EQ(rangeGot[13], 0);

    MaskOverlayDesc od = {};
    od.channel = MaskChannel::Alpha;
    od.opacity = 1.0f;
    od.overlayColor[0] = 1.0f;
    od.overlayColor[1] = 0.0f;
    od.overlayColor[2] = 0.0f;
    od.overlayColor[3] = 1.0f;
    proc.DispatchMaskOverlay(core->GetImmediateContext(), src, overlayOut, od);

    const auto overlayGot = ReadbackRgba8(*core, overlayOut);
    RequireByteNear(overlayGot[3], 0, 1, "overlay a0", 3);
    RequireByteNear(overlayGot[7], 80, 2, "overlay a1", 7);
    RequireByteNear(overlayGot[11], 160, 2, "overlay a2", 11);
    RequireByteNear(overlayGot[15], 255, 1, "overlay a3", 15);
}

TEST(ProcessingThreshold, HeatmapAndClassColorMapReadback) {
    REQUIRE_CORE(core);
    Fixture fx(core);
    if (!fx.processing.SupportsRgba8Uav()) {
        std::cout << "R8G8B8A8 UAV not supported; skipping\n";
        return;
    }

    const std::vector<uint8_t> pixels = {
        0, 0, 0, 255,
        1, 0, 0, 255,
        2, 0, 0, 255,
        3, 0, 0, 255,
    };
    auto src = CreateTexture2DFromRGBA(*core, pixels.data(), 4, 1, D3D11_BIND_SHADER_RESOURCE);

    D3D11ThresholdProcessor proc;
    proc.Initialize(fx.processing);
    auto classOut = proc.CreateOutputTexture(*core, 4, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
    auto heatOut = proc.CreateOutputTexture(*core, 4, 1, DXGI_FORMAT_R8G8B8A8_UNORM);

    ClassColorMapDesc cd = {};
    cd.channel = MaskChannel::Red;
    cd.classScale = 255.0f;
    cd.classCount = 4;
    proc.DispatchClassColorMap(core->GetImmediateContext(), src, classOut, cd);

    const auto classGot = ReadbackRgba8(*core, classOut);
    // class 0 is black; class 1 in the built-in palette is red-like.
    RequireByteNear(classGot[0], 0, 1, "class0 r", 0);
    CHECK(classGot[4] > 200);

    ConfidenceHeatmapDesc hd = {};
    hd.channel = MaskChannel::Red;
    hd.mode = HeatmapMode::Grayscale;
    hd.minValue = 0.0f;
    hd.maxValue = 3.0f / 255.0f;
    hd.opacity = 1.0f;
    proc.DispatchConfidenceHeatmap(core->GetImmediateContext(), src, heatOut, hd);

    const auto heatGot = ReadbackRgba8(*core, heatOut);
    CHECK(heatGot[0] < heatGot[4]);
    CHECK(heatGot[4] < heatGot[8]);
    CHECK(heatGot[8] < heatGot[12]);
}
