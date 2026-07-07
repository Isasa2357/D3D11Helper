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

bool HasProcessingShader(const std::filesystem::path& dir) {
    std::error_code ec;
    return std::filesystem::exists(dir / "FusedRgbToRgbResize.hlsl", ec) && !ec;
}

std::filesystem::path ProcessingShaderDir() {
    const auto namespacedRuntimeDir = std::filesystem::current_path() / "D3D11Helper" / "shaders" / "D3D11Processing";
    if (HasProcessingShader(namespacedRuntimeDir)) {
        return namespacedRuntimeDir;
    }

    const auto legacyRuntimeDir = std::filesystem::current_path() / "shaders" / "D3D11Processing";
    if (HasProcessingShader(legacyRuntimeDir)) {
        return legacyRuntimeDir;
    }

#ifdef D3D11HELPER_TEST_SOURCE_DIR
    const auto sourceDir = std::filesystem::u8path(D3D11HELPER_TEST_SOURCE_DIR).parent_path() / "shaders" / "D3D11Processing";
    if (HasProcessingShader(sourceDir)) {
        return sourceDir;
    }
#endif

    return namespacedRuntimeDir;
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

void RequireBytesEqual(
    const std::vector<uint8_t>& actual,
    const std::vector<uint8_t>& expected,
    const char* label) {

    if (actual.size() != expected.size()) {
        std::ostringstream os;
        os << label << ": size mismatch actual=" << actual.size()
           << " expected=" << expected.size();
        ::Test::Fail(__FILE__, __LINE__, os.str());
    }

    for (size_t i = 0; i < expected.size(); ++i) {
        if (actual[i] != expected[i]) {
            std::ostringstream os;
            os << label << ": byte mismatch at index " << i
               << " actual=" << static_cast<int>(actual[i])
               << " expected=" << static_cast<int>(expected[i]);
            ::Test::Fail(__FILE__, __LINE__, os.str());
        }
    }
}

std::vector<uint8_t> MakePointResizeExpected2x2To4x4(const std::vector<uint8_t>& src) {
    std::vector<uint8_t> expected(4u * 4u * 4u);
    for (UINT y = 0; y < 4; ++y) {
        for (UINT x = 0; x < 4; ++x) {
            const UINT sx = x / 2u;
            const UINT sy = y / 2u;
            const size_t srcIndex = static_cast<size_t>(sy * 2u + sx) * 4u;
            const size_t dstIndex = static_cast<size_t>(y * 4u + x) * 4u;
            std::memcpy(expected.data() + dstIndex, src.data() + srcIndex, 4u);
        }
    }
    return expected;
}

} // namespace

TEST(ProcessingFusedPipeline, ShaderCompile) {
    REQUIRE_CORE(core);
    Fixture fx(core);

    D3D11ProcessingShaderCache cache;
    cache.Initialize(fx.processing);

    CHECK(!cache.GetComputeShader("FusedRgbToRgbResize.hlsl").Empty());
    CHECK(!cache.GetComputeShader("FusedYuv420ToRgbResize.hlsl").Empty());
}

TEST(ProcessingFusedPipeline, RgbaPointResizeReadbackMatchesNearestPixels) {
    REQUIRE_CORE(core);
    Fixture fx(core);

    if (!fx.processing.SupportsRgba8Uav()) {
        std::cout << "R8G8B8A8 UAV not supported; skipping\n";
        return;
    }

    const std::vector<uint8_t> srcPixels = {
        255,   0,   0, 255,   0, 255,   0, 255,
          0,   0, 255, 255, 255, 255,   0, 255,
    };
    const auto expected = MakePointResizeExpected2x2To4x4(srcPixels);

    auto src = CreateTexture2DFromRGBA(*core, srcPixels.data(), 2, 2, D3D11_BIND_SHADER_RESOURCE);

    D3D11FusedProcessor fused;
    fused.Initialize(fx.processing);
    auto dst = fused.CreateOutputTexture(*core, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM);

    FusedConvertResizeDesc desc = {};
    desc.srcFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.dstFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.filter = ProcessingFilter::Point;

    fused.DispatchConvertResize(core->GetImmediateContext(), src, dst, desc);
    RequireBytesEqual(ReadbackRgba8(*core, dst), expected, "fused RGBA point resize 2x2 -> 4x4");
}

TEST(ProcessingFusedPipeline, RejectsInvalidOutputSize) {
    REQUIRE_CORE(core);
    Fixture fx(core);

    D3D11FusedProcessor fused;
    fused.Initialize(fx.processing);
    CHECK_THROWS(fused.CreateOutputTexture(*core, 0, 4, DXGI_FORMAT_R8G8B8A8_UNORM));
    CHECK_THROWS(fused.CreateOutputTexture(*core, 4, 0, DXGI_FORMAT_R8G8B8A8_UNORM));
}
