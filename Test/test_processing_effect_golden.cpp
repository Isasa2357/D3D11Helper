//
// test_processing_effect_golden.cpp - Golden pixel tests for D3D11Processing effect passes.
//
#include "TestUtil.hpp"

#include <D3D11Helper/D3D11Processing/D3D11Processing.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace D3D11CoreLib;
using namespace D3D11CoreLib::Processing;

namespace {

std::filesystem::path ProcessingShaderDir() {
    const auto runtimeDir = std::filesystem::current_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(runtimeDir / "ColorAdjustRgba.hlsl")) {
        return runtimeDir;
    }
#ifdef D3D11HELPER_TEST_SOURCE_DIR
    const auto sourceDir = std::filesystem::u8path(D3D11HELPER_TEST_SOURCE_DIR).parent_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(sourceDir / "ColorAdjustRgba.hlsl")) {
        return sourceDir;
    }
#endif
    return runtimeDir;
}

struct Fixture {
    std::shared_ptr<D3D11Core> core;
    D3D11ProcessingContext processing;

    Fixture() : core(TestUtil::MakeCore()) {
        processing.Initialize(*core, ProcessingShaderDir());
    }
};

uint8_t ClampByte(int v) {
    return static_cast<uint8_t>(std::max(0, std::min(255, v)));
}

uint8_t FloatToByte(float v) {
    v = std::max(0.0f, std::min(1.0f, v));
    return ClampByte(static_cast<int>(std::lround(v * 255.0f)));
}

std::vector<uint8_t> ReadbackRgba8(D3D11Core& core, D3D11Resource& texture) {
    auto image = ReadbackTexture2DToCpuImage(core, texture);
    ValidateCpuImage(image, "ReadbackRgba8");
    const auto& plane = image.planes[0];
    const UINT rowBytes = GetPackedRowPitch(image.width, image.format);
    return PackRows(image.pixels.data() + static_cast<size_t>(plane.offsetBytes),
                    plane.rowPitch,
                    rowBytes,
                    plane.height);
}

void RequireByteNear(uint8_t actual, uint8_t expected, uint8_t tolerance, const char* label, size_t index) {
    const int diff = std::abs(int(actual) - int(expected));
    if (diff > int(tolerance)) {
        std::ostringstream os;
        os << label << ": byte mismatch at index " << index
           << " actual=" << int(actual)
           << " expected=" << int(expected)
           << " tolerance=" << int(tolerance);
        throw std::runtime_error(os.str());
    }
}

void RequireBytesNear(const std::vector<uint8_t>& actual, const std::vector<uint8_t>& expected, uint8_t tolerance, const char* label) {
    if (actual.size() != expected.size()) {
        throw std::runtime_error(std::string(label) + ": size mismatch");
    }
    for (size_t i = 0; i < expected.size(); ++i) {
        RequireByteNear(actual[i], expected[i], tolerance, label, i);
    }
}

std::vector<uint8_t> MakeSolidRgba(UINT width, UINT height, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4u);
    for (size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i + 0] = r;
        pixels[i + 1] = g;
        pixels[i + 2] = b;
        pixels[i + 3] = a;
    }
    return pixels;
}

} // namespace

int main() {
    TEST_RUN("ColorAdjust brightness golden", {
        Fixture fx;
        if (!fx.processing.SupportsRgba8Uav()) {
            TestUtil::Log("Skipping: R8G8B8A8 UAV not supported");
            return;
        }

        const std::vector<uint8_t> srcPixels = {
             10,  20,  30, 255,
            100, 120, 140, 200,
        };
        auto src = CreateTexture2DFromRGBA(*fx.core, srcPixels.data(), 2, 1, D3D11_BIND_SHADER_RESOURCE);

        D3D11ColorAdjuster adjuster;
        adjuster.Initialize(fx.processing);
        auto dst = adjuster.CreateOutputTexture(*fx.core, 2, 1, DXGI_FORMAT_R8G8B8A8_UNORM);

        ColorAdjustDesc desc{};
        desc.brightness = 0.10f;
        adjuster.DispatchColorAdjust(fx.core->GetImmediateContext(), src, dst, desc);

        const std::vector<uint8_t> expected = {
            FloatToByte(10.0f / 255.0f + 0.10f),
            FloatToByte(20.0f / 255.0f + 0.10f),
            FloatToByte(30.0f / 255.0f + 0.10f),
            255,
            FloatToByte(100.0f / 255.0f + 0.10f),
            FloatToByte(120.0f / 255.0f + 0.10f),
            FloatToByte(140.0f / 255.0f + 0.10f),
            200,
        };
        RequireBytesNear(ReadbackRgba8(*fx.core, dst), expected, 2, "color adjust brightness");
    });

    TEST_RUN("Kernel custom average center golden", {
        Fixture fx;
        if (!fx.processing.SupportsRgba8Uav()) {
            TestUtil::Log("Skipping: R8G8B8A8 UAV not supported");
            return;
        }

        const std::vector<uint8_t> srcPixels = {
             10, 10, 10, 255,  20, 20, 20, 255,  30, 30, 30, 255,
             40, 40, 40, 255,  50, 50, 50, 255,  60, 60, 60, 255,
             70, 70, 70, 255,  80, 80, 80, 255,  90, 90, 90, 255,
        };
        auto src = CreateTexture2DFromRGBA(*fx.core, srcPixels.data(), 3, 3, D3D11_BIND_SHADER_RESOURCE);

        D3D11KernelFilter filter;
        filter.Initialize(fx.processing);
        auto dst = filter.CreateOutputTexture(*fx.core, 3, 3, DXGI_FORMAT_R8G8B8A8_UNORM);

        KernelFilterDesc desc{};
        desc.mode = KernelFilterMode::Custom3x3;
        desc.edgeMode = KernelEdgeMode::Clamp;
        desc.preserveAlpha = true;
        for (float& k : desc.kernel) {
            k = 1.0f / 9.0f;
        }
        filter.DispatchKernelFilter(fx.core->GetImmediateContext(), src, dst, desc);

        const auto got = ReadbackRgba8(*fx.core, dst);
        const size_t center = static_cast<size_t>(1u * 3u + 1u) * 4u;
        RequireByteNear(got[center + 0], 50, 2, "kernel average center r", center + 0);
        RequireByteNear(got[center + 1], 50, 2, "kernel average center g", center + 1);
        RequireByteNear(got[center + 2], 50, 2, "kernel average center b", center + 2);
        RequireByteNear(got[center + 3], 255, 1, "kernel average center a", center + 3);
    });

    TEST_RUN("Mask multiply rgb red channel golden", {
        Fixture fx;
        if (!fx.processing.SupportsRgba8Uav()) {
            TestUtil::Log("Skipping: R8G8B8A8 UAV not supported");
            return;
        }

        const auto srcPixels = MakeSolidRgba(3, 1, 100, 50, 200, 255);
        const std::vector<uint8_t> maskPixels = {
              0, 0, 0, 255,
            128, 0, 0, 255,
            255, 0, 0, 255,
        };
        auto src = CreateTexture2DFromRGBA(*fx.core, srcPixels.data(), 3, 1, D3D11_BIND_SHADER_RESOURCE);
        auto mask = CreateTexture2DFromRGBA(*fx.core, maskPixels.data(), 3, 1, D3D11_BIND_SHADER_RESOURCE);

        D3D11MaskProcessor processor;
        processor.Initialize(fx.processing);
        auto dst = processor.CreateOutputTexture(*fx.core, 3, 1, DXGI_FORMAT_R8G8B8A8_UNORM);

        MaskApplyDesc desc{};
        desc.mode = MaskApplyMode::MultiplyRgb;
        desc.channel = MaskChannel::Red;
        processor.DispatchApplyMask(fx.core->GetImmediateContext(), src, mask, dst, desc);

        const std::vector<uint8_t> expected = {
              0,  0,   0, 255,
             50, 25, 100, 255,
            100, 50, 200, 255,
        };
        RequireBytesNear(ReadbackRgba8(*fx.core, dst), expected, 2, "mask multiply rgb");
    });

    TEST_RUN("Threshold red channel golden", {
        Fixture fx;
        if (!fx.processing.SupportsRgba8Uav()) {
            TestUtil::Log("Skipping: R8G8B8A8 UAV not supported");
            return;
        }

        const std::vector<uint8_t> srcPixels = {
             64, 0, 0, 255,   128, 0, 0, 255,
            192, 0, 0, 255,     0, 0, 0, 255,
        };
        auto src = CreateTexture2DFromRGBA(*fx.core, srcPixels.data(), 2, 2, D3D11_BIND_SHADER_RESOURCE);

        D3D11ThresholdProcessor processor;
        processor.Initialize(fx.processing);
        auto dst = processor.CreateOutputTexture(*fx.core, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM);

        ThresholdDesc desc{};
        desc.channel = MaskChannel::Red;
        desc.threshold = 0.5f;
        desc.foregroundColor[0] = 0.0f; desc.foregroundColor[1] = 1.0f; desc.foregroundColor[2] = 0.0f; desc.foregroundColor[3] = 1.0f;
        desc.backgroundColor[0] = 1.0f; desc.backgroundColor[1] = 0.0f; desc.backgroundColor[2] = 0.0f; desc.backgroundColor[3] = 1.0f;
        processor.DispatchThreshold(fx.core->GetImmediateContext(), src, dst, desc);

        const std::vector<uint8_t> expected = {
            255,   0, 0, 255,     0, 255, 0, 255,
              0, 255, 0, 255,   255,   0, 0, 255,
        };
        RequireBytesNear(ReadbackRgba8(*fx.core, dst), expected, 0, "threshold red channel");
    });

    return TestUtil::Result("ProcessingEffectGolden");
}
