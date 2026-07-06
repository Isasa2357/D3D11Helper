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
    if (std::filesystem::exists(runtimeDir / "KernelFilterRgba.hlsl")) {
        return runtimeDir;
    }

#ifdef D3D11HELPER_TEST_SOURCE_DIR
    const auto sourceDir = std::filesystem::u8path(D3D11HELPER_TEST_SOURCE_DIR).parent_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(sourceDir / "KernelFilterRgba.hlsl")) {
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

void RequireBytesEqual(const std::vector<uint8_t>& actual, const std::vector<uint8_t>& expected, const char* label) {
    if (actual.size() != expected.size()) {
        std::ostringstream os;
        os << label << ": size mismatch actual=" << actual.size() << " expected=" << expected.size();
        ::Test::Fail(__FILE__, __LINE__, os.str());
    }
    for (size_t i = 0; i < expected.size(); ++i) {
        if (actual[i] != expected[i]) {
            std::ostringstream os;
            os << label << ": byte mismatch at index " << i
               << " actual=" << int(actual[i])
               << " expected=" << int(expected[i]);
            ::Test::Fail(__FILE__, __LINE__, os.str());
        }
    }
}

} // namespace

TEST(ProcessingKernelFilter, ShaderCompile) {
    REQUIRE_CORE(core);

    Fixture fx(core);

    D3D11ProcessingShaderCache cache;
    cache.Initialize(fx.processing);

    CHECK(!cache.GetComputeShader("KernelFilterRgba.hlsl").Empty());
}

TEST(ProcessingKernelFilter, CustomIdentityCopies) {
    REQUIRE_CORE(core);

    Fixture fx(core);

    if (!fx.processing.SupportsRgba8Uav()) {
        std::cout << "R8G8B8A8 UAV not supported; skipping\n";
        return;
    }

    const std::vector<uint8_t> srcPixels = {
        10, 20, 30, 255,   40, 50, 60, 128,
        70, 80, 90, 255,   100, 110, 120, 64,
    };

    auto src = CreateTexture2DFromRGBA(*core, srcPixels.data(), 2, 2, D3D11_BIND_SHADER_RESOURCE);

    D3D11KernelFilter filter;
    filter.Initialize(fx.processing);
    auto dst = filter.CreateOutputTexture(*core, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM);

    KernelFilterDesc desc = {};
    desc.mode = KernelFilterMode::Custom3x3;
    desc.edgeMode = KernelEdgeMode::Clamp;
    desc.preserveAlpha = true;

    filter.DispatchKernelFilter(core->GetImmediateContext(), src, dst, desc);

    RequireBytesEqual(ReadbackRgba8(*core, dst), srcPixels, "custom identity kernel");
}

TEST(ProcessingKernelFilter, EdgeDetectUniformIsZeroRgb) {
    REQUIRE_CORE(core);

    Fixture fx(core);

    if (!fx.processing.SupportsRgba8Uav()) {
        std::cout << "R8G8B8A8 UAV not supported; skipping\n";
        return;
    }

    std::vector<uint8_t> srcPixels(3u * 3u * 4u, 0);
    for (size_t i = 0; i < srcPixels.size(); i += 4) {
        srcPixels[i + 0] = 120;
        srcPixels[i + 1] = 120;
        srcPixels[i + 2] = 120;
        srcPixels[i + 3] = 255;
    }

    auto src = CreateTexture2DFromRGBA(*core, srcPixels.data(), 3, 3, D3D11_BIND_SHADER_RESOURCE);

    D3D11KernelFilter filter;
    filter.Initialize(fx.processing);
    auto dst = filter.CreateOutputTexture(*core, 3, 3, DXGI_FORMAT_R8G8B8A8_UNORM);

    KernelFilterDesc desc = {};
    desc.mode = KernelFilterMode::EdgeDetect;
    desc.edgeMode = KernelEdgeMode::Clamp;
    desc.preserveAlpha = true;

    filter.DispatchKernelFilter(core->GetImmediateContext(), src, dst, desc);

    const auto got = ReadbackRgba8(*core, dst);
    for (size_t i = 0; i < got.size(); i += 4) {
        CHECK_EQ(got[i + 0], 0);
        CHECK_EQ(got[i + 1], 0);
        CHECK_EQ(got[i + 2], 0);
        CHECK_EQ(got[i + 3], 255);
    }
}
