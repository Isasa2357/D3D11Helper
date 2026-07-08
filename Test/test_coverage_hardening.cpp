//
// test_coverage_hardening.cpp - additional coverage-focused validation tests.
//
#include "TestUtil.hpp"

#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Copy.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Resolve.hpp>
#include <D3D11Helper/D3D11Processing/D3D11Processing.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace D3D11CoreLib;
using namespace D3D11CoreLib::Processing;

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

    explicit Fixture(std::filesystem::path shaderDir = {}) : core(TestUtil::MakeCore()) {
        processing.Initialize(*core, shaderDir.empty() ? ProcessingShaderDir() : std::move(shaderDir));
    }
};

ComPtr<ID3D11Texture2D> CreateTexture(
    ID3D11Device* device,
    UINT width,
    UINT height,
    DXGI_FORMAT format,
    UINT bindFlags,
    UINT sampleCount = 1) {

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = sampleCount;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = bindFlags;

    ComPtr<ID3D11Texture2D> texture;
    D3D11CORE_THROW_IF_FAILED(device->CreateTexture2D(&desc, nullptr, &texture));
    return texture;
}

ComPtr<ID3D11Buffer> CreateBuffer(ID3D11Device* device, UINT byteWidth) {
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = byteWidth;
    desc.Usage = D3D11_USAGE_DEFAULT;
    ComPtr<ID3D11Buffer> buffer;
    D3D11CORE_THROW_IF_FAILED(device->CreateBuffer(&desc, nullptr, &buffer));
    return buffer;
}

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

    D3D11_MAPPED_SUBRESOURCE mapped{};
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
        throw std::runtime_error(os.str());
    }
}

void RequireBytesEqual(const std::vector<uint8_t>& actual, const std::vector<uint8_t>& expected, const char* label) {
    if (actual.size() != expected.size()) {
        throw std::runtime_error(std::string(label) + ": size mismatch");
    }
    for (size_t i = 0; i < expected.size(); ++i) {
        if (actual[i] != expected[i]) {
            std::ostringstream os;
            os << label << ": byte mismatch at index " << i
               << " actual=" << int(actual[i])
               << " expected=" << int(expected[i]);
            throw std::runtime_error(os.str());
        }
    }
}

} // namespace

int main() {
    TEST_RUN("ProcessingShaderCache error paths", {
        D3D11ProcessingShaderCache uninitialized;
        ExpectThrows("uninitialized shader cache", [&] { (void)uninitialized.GetComputeShader("missing.hlsl"); });

        Fixture fx;
        D3D11ProcessingShaderCache cache;
        cache.Initialize(fx.processing);
        ExpectThrows("missing shader", [&] { (void)cache.GetComputeShader("ThisShaderDoesNotExist.hlsl"); });

        const auto tempDir = std::filesystem::temp_directory_path() / "D3D11Helper_BadProcessingShader";
        std::filesystem::create_directories(tempDir);
        const auto badShader = tempDir / "BadShader.hlsl";
        {
            std::ofstream os(badShader, std::ios::binary);
            os << "this is not valid hlsl";
        }

        Fixture badFx(tempDir);
        D3D11ProcessingShaderCache badCache;
        badCache.Initialize(badFx.processing);
        ExpectThrows("bad shader", [&] { (void)badCache.GetComputeShader("BadShader.hlsl"); });
        badCache.Clear();
        std::filesystem::remove_all(tempDir);
    });

    TEST_RUN("Copy and resolve boundary validation", {
        auto core = TestUtil::MakeCore();
        auto* device = core->GetDevice();
        auto* context = core->GetImmediateContext();

        auto texA = CreateTexture(device, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
        auto texB = CreateTexture(device, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
        auto bufA = CreateBuffer(device, 16);
        auto bufB = CreateBuffer(device, 16);

        ExpectThrows("CopyTexture2D null dst", [&] { CopyTexture2D(context, nullptr, texA.Get()); });
        ExpectThrows("CopyTexture2DRegion dst bounds", [&] {
            D3D11Texture2DRegion r{};
            r.width = 2;
            r.height = 2;
            r.dstX = 3;
            r.dstY = 0;
            CopyTexture2DRegion(context, texB.Get(), texA.Get(), r);
        });
        ExpectThrows("CopyBufferRegion out of range", [&] {
            D3D11BufferCopyRegion r{};
            r.srcOffset = 15;
            r.dstOffset = 0;
            r.sizeBytes = 2;
            CopyBufferRegion(context, bufB.Get(), bufA.Get(), r);
        });
        ExpectThrows("ResolveTexture2D source not multisampled", [&] { ResolveTexture2D(context, texB.Get(), texA.Get()); });
    });

    TEST_RUN("Mask apply mode golden cases", {
        Fixture fx;
        if (!fx.processing.SupportsRgba8Uav()) {
            TestUtil::Log("R8G8B8A8 UAV not supported; skipping mask golden cases");
            return;
        }

        const std::vector<uint8_t> srcPixels = { 100, 150, 200, 128 };
        const std::vector<uint8_t> maskPixels = { 0, 0, 0, 128 };
        auto src = CreateTexture2DFromRGBA(*fx.core, srcPixels.data(), 1, 1, D3D11_BIND_SHADER_RESOURCE);
        auto mask = CreateTexture2DFromRGBA(*fx.core, maskPixels.data(), 1, 1, D3D11_BIND_SHADER_RESOURCE);

        D3D11MaskProcessor proc;
        proc.Initialize(fx.processing);

        auto run = [&](MaskApplyMode mode) {
            auto dst = proc.CreateOutputTexture(*fx.core, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
            MaskApplyDesc desc{};
            desc.mode = mode;
            desc.channel = MaskChannel::Alpha;
            desc.strength = 1.0f;
            proc.DispatchApplyMask(fx.core->GetImmediateContext(), src, mask, dst, desc);
            return ReadbackRgba8(*fx.core, dst);
        };

        const auto multiplyRgb = run(MaskApplyMode::MultiplyRgb);
        RequireByteNear(multiplyRgb[0], 50, 2, "multiply rgb r", 0);
        RequireByteNear(multiplyRgb[1], 75, 2, "multiply rgb g", 1);
        RequireByteNear(multiplyRgb[2], 100, 2, "multiply rgb b", 2);
        RequireByteNear(multiplyRgb[3], 128, 1, "multiply rgb a", 3);

        const auto multiplyRgba = run(MaskApplyMode::MultiplyRgba);
        RequireByteNear(multiplyRgba[0], 50, 2, "multiply rgba r", 0);
        RequireByteNear(multiplyRgba[1], 75, 2, "multiply rgba g", 1);
        RequireByteNear(multiplyRgba[2], 100, 2, "multiply rgba b", 2);
        RequireByteNear(multiplyRgba[3], 64, 2, "multiply rgba a", 3);
    });

    TEST_RUN("Threshold invert golden case", {
        Fixture fx;
        if (!fx.processing.SupportsRgba8Uav()) {
            TestUtil::Log("R8G8B8A8 UAV not supported; skipping threshold golden case");
            return;
        }

        const std::vector<uint8_t> srcPixels = {
            0,   0, 0, 255,
            255, 0, 0, 255,
        };
        auto src = CreateTexture2DFromRGBA(*fx.core, srcPixels.data(), 2, 1, D3D11_BIND_SHADER_RESOURCE);

        D3D11ThresholdProcessor proc;
        proc.Initialize(fx.processing);
        auto dst = proc.CreateOutputTexture(*fx.core, 2, 1, DXGI_FORMAT_R8G8B8A8_UNORM);

        ThresholdDesc desc{};
        desc.channel = MaskChannel::Red;
        desc.threshold = 0.5f;
        desc.invert = true;
        desc.foregroundColor[0] = 1.0f;
        desc.foregroundColor[1] = 1.0f;
        desc.foregroundColor[2] = 1.0f;
        desc.foregroundColor[3] = 1.0f;
        desc.backgroundColor[0] = 0.0f;
        desc.backgroundColor[1] = 0.0f;
        desc.backgroundColor[2] = 0.0f;
        desc.backgroundColor[3] = 1.0f;
        proc.DispatchThreshold(fx.core->GetImmediateContext(), src, dst, desc);

        const std::vector<uint8_t> expected = {
            255, 255, 255, 255,
            0,   0,   0,   255,
        };
        RequireBytesEqual(ReadbackRgba8(*fx.core, dst), expected, "threshold invert");
    });

    return TestUtil::Result("CoverageHardening");
}
