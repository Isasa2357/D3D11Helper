//
// test_processing_mask_composition_golden.cpp
// Golden pixel tests for D3D11Processing mask composition passes.
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
    if (std::filesystem::exists(runtimeDir / "MaskBlendRgba.hlsl")) {
        return runtimeDir;
    }
#ifdef D3D11HELPER_TEST_SOURCE_DIR
    const auto sourceDir = std::filesystem::u8path(D3D11HELPER_TEST_SOURCE_DIR).parent_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(sourceDir / "MaskBlendRgba.hlsl")) {
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

uint8_t FloatToByte(float v) {
    v = std::max(0.0f, std::min(1.0f, v));
    return static_cast<uint8_t>(std::max(0, std::min(255, static_cast<int>(std::lround(v * 255.0f)))));
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

std::vector<uint8_t> SolidRgba(UINT width, UINT height, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
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
    TEST_RUN("MaskBlend red channel opacity golden", {
        Fixture fx;
        if (!fx.processing.SupportsRgba8Uav()) {
            TestUtil::Log("Skipping: R8G8B8A8 UAV not supported");
            return;
        }

        const auto basePixels = SolidRgba(3, 1, 10, 20, 30, 255);
        const auto overlayPixels = SolidRgba(3, 1, 210, 220, 230, 255);
        const std::vector<uint8_t> maskPixels = {
              0, 0, 0, 255,
            128, 0, 0, 255,
            255, 0, 0, 255,
        };

        auto base = CreateTexture2DFromRGBA(*fx.core, basePixels.data(), 3, 1, D3D11_BIND_SHADER_RESOURCE);
        auto overlay = CreateTexture2DFromRGBA(*fx.core, overlayPixels.data(), 3, 1, D3D11_BIND_SHADER_RESOURCE);
        auto mask = CreateTexture2DFromRGBA(*fx.core, maskPixels.data(), 3, 1, D3D11_BIND_SHADER_RESOURCE);

        D3D11MaskProcessor processor;
        processor.Initialize(fx.processing);
        auto dst = processor.CreateOutputTexture(*fx.core, 3, 1, DXGI_FORMAT_R8G8B8A8_UNORM);

        MaskBlendDesc desc{};
        desc.channel = MaskChannel::Red;
        desc.opacity = 1.0f;
        processor.DispatchBlendByMask(fx.core->GetImmediateContext(), base, overlay, mask, dst, desc);

        const float half = 128.0f / 255.0f;
        const std::vector<uint8_t> expected = {
             10,  20,  30, 255,
            FloatToByte((10.0f / 255.0f) * (1.0f - half) + (210.0f / 255.0f) * half),
            FloatToByte((20.0f / 255.0f) * (1.0f - half) + (220.0f / 255.0f) * half),
            FloatToByte((30.0f / 255.0f) * (1.0f - half) + (230.0f / 255.0f) * half),
            255,
            210, 220, 230, 255,
        };
        RequireBytesNear(ReadbackRgba8(*fx.core, dst), expected, 2, "mask blend red");
    });

    TEST_RUN("MaskCombine max red green golden", {
        Fixture fx;
        if (!fx.processing.SupportsRgba8Uav()) {
            TestUtil::Log("Skipping: R8G8B8A8 UAV not supported");
            return;
        }

        const std::vector<uint8_t> maskA = {
             64, 0, 0, 255,
            128, 0, 0, 255,
            255, 0, 0, 255,
        };
        const std::vector<uint8_t> maskB = {
              0, 128, 0, 255,
              0,  64, 0, 255,
              0, 128, 0, 255,
        };
        auto a = CreateTexture2DFromRGBA(*fx.core, maskA.data(), 3, 1, D3D11_BIND_SHADER_RESOURCE);
        auto b = CreateTexture2DFromRGBA(*fx.core, maskB.data(), 3, 1, D3D11_BIND_SHADER_RESOURCE);

        D3D11MaskProcessor processor;
        processor.Initialize(fx.processing);
        auto dst = processor.CreateOutputTexture(*fx.core, 3, 1, DXGI_FORMAT_R8G8B8A8_UNORM);

        MaskCombineDesc desc{};
        desc.mode = MaskCombineMode::Max;
        desc.channelA = MaskChannel::Red;
        desc.channelB = MaskChannel::Green;
        processor.DispatchCombineMasks(fx.core->GetImmediateContext(), a, b, dst, desc);

        const std::vector<uint8_t> expected = {
            128, 128, 128, 128,
            128, 128, 128, 128,
            255, 255, 255, 255,
        };
        RequireBytesNear(ReadbackRgba8(*fx.core, dst), expected, 1, "mask combine max");
    });

    TEST_RUN("MaskInvert blue channel golden", {
        Fixture fx;
        if (!fx.processing.SupportsRgba8Uav()) {
            TestUtil::Log("Skipping: R8G8B8A8 UAV not supported");
            return;
        }

        const std::vector<uint8_t> maskPixels = {
            0, 0,   0, 255,
            0, 0, 128, 255,
            0, 0, 255, 255,
        };
        auto mask = CreateTexture2DFromRGBA(*fx.core, maskPixels.data(), 3, 1, D3D11_BIND_SHADER_RESOURCE);

        D3D11MaskProcessor processor;
        processor.Initialize(fx.processing);
        auto dst = processor.CreateOutputTexture(*fx.core, 3, 1, DXGI_FORMAT_R8G8B8A8_UNORM);

        MaskInvertDesc desc{};
        desc.channel = MaskChannel::Blue;
        processor.DispatchInvertMask(fx.core->GetImmediateContext(), mask, dst, desc);

        const std::vector<uint8_t> expected = {
            255, 255, 255, 255,
            127, 127, 127, 127,
              0,   0,   0,   0,
        };
        RequireBytesNear(ReadbackRgba8(*fx.core, dst), expected, 1, "mask invert blue");
    });

    return TestUtil::Result("ProcessingMaskCompositionGolden");
}
