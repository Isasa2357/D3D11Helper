//
// test_processing_pyramid_golden.cpp - Golden pixel tests for D3D11Processing pyramid passes.
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
    if (std::filesystem::exists(runtimeDir / "PyramidDownsample2xRgba.hlsl")) {
        return runtimeDir;
    }
#ifdef D3D11HELPER_TEST_SOURCE_DIR
    const auto sourceDir = std::filesystem::u8path(D3D11HELPER_TEST_SOURCE_DIR).parent_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(sourceDir / "PyramidDownsample2xRgba.hlsl")) {
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

std::vector<uint8_t> MakeGrayRgba(const std::vector<uint8_t>& values) {
    std::vector<uint8_t> pixels;
    pixels.reserve(values.size() * 4u);
    for (uint8_t v : values) {
        pixels.push_back(v);
        pixels.push_back(v);
        pixels.push_back(v);
        pixels.push_back(255);
    }
    return pixels;
}

std::vector<uint8_t> MakeExpectedPointUpsample2x(
    const std::vector<uint8_t>& src,
    UINT srcWidth,
    UINT srcHeight) {

    std::vector<uint8_t> out(static_cast<size_t>(srcWidth) * 2u * srcHeight * 2u * 4u);
    const UINT dstWidth = srcWidth * 2u;
    for (UINT y = 0; y < srcHeight * 2u; ++y) {
        for (UINT x = 0; x < srcWidth * 2u; ++x) {
            const UINT sx = x / 2u;
            const UINT sy = y / 2u;
            const size_t srcIndex = static_cast<size_t>(sy * srcWidth + sx) * 4u;
            const size_t dstIndex = static_cast<size_t>(y * dstWidth + x) * 4u;
            out[dstIndex + 0] = src[srcIndex + 0];
            out[dstIndex + 1] = src[srcIndex + 1];
            out[dstIndex + 2] = src[srcIndex + 2];
            out[dstIndex + 3] = src[srcIndex + 3];
        }
    }
    return out;
}

} // namespace

int main() {
    TEST_RUN("Pyramid downsample 4x4 average golden", {
        Fixture fx;
        if (!fx.processing.SupportsRgba8Uav()) {
            TestUtil::Log("Skipping: R8G8B8A8 UAV not supported");
            return;
        }

        const auto srcPixels = MakeGrayRgba({
             10,  20,  50,  60,
             30,  40,  70,  80,
            100, 110, 140, 150,
            120, 130, 160, 170,
        });
        auto src = CreateTexture2DFromRGBA(*fx.core, srcPixels.data(), 4, 4, D3D11_BIND_SHADER_RESOURCE);

        D3D11PyramidProcessor pyramid;
        pyramid.Initialize(fx.processing);
        auto dst = pyramid.CreateDownsampledTexture(*fx.core, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM);

        PyramidDownsampleDesc desc{};
        desc.edgeMode = PyramidEdgeMode::Clamp;
        pyramid.DispatchDownsample2x(fx.core->GetImmediateContext(), src, dst, desc);

        const auto expected = MakeGrayRgba({ 25, 65, 115, 155 });
        RequireBytesNear(ReadbackRgba8(*fx.core, dst), expected, 2, "pyramid downsample 4x4");
    });

    TEST_RUN("Pyramid downsample 3x3 constant edge golden", {
        Fixture fx;
        if (!fx.processing.SupportsRgba8Uav()) {
            TestUtil::Log("Skipping: R8G8B8A8 UAV not supported");
            return;
        }

        const auto srcPixels = MakeGrayRgba({
            10, 20, 30,
            40, 50, 60,
            70, 80, 90,
        });
        auto src = CreateTexture2DFromRGBA(*fx.core, srcPixels.data(), 3, 3, D3D11_BIND_SHADER_RESOURCE);

        D3D11PyramidProcessor pyramid;
        pyramid.Initialize(fx.processing);
        auto dst = pyramid.CreateDownsampledTexture(*fx.core, 3, 3, DXGI_FORMAT_R8G8B8A8_UNORM);

        PyramidDownsampleDesc desc{};
        desc.edgeMode = PyramidEdgeMode::Constant;
        desc.borderColor[0] = 0.0f;
        desc.borderColor[1] = 0.0f;
        desc.borderColor[2] = 0.0f;
        desc.borderColor[3] = 0.0f;
        pyramid.DispatchDownsample2x(fx.core->GetImmediateContext(), src, dst, desc);

        const std::vector<uint8_t> expected = {
            30, 30, 30, 255,
            22, 22, 22,  64,
            37, 37, 37,  64,
            22, 22, 22,  64,
        };
        RequireBytesNear(ReadbackRgba8(*fx.core, dst), expected, 2, "pyramid downsample 3x3 constant");
    });

    TEST_RUN("Pyramid upsample point repeat golden", {
        Fixture fx;
        if (!fx.processing.SupportsRgba8Uav()) {
            TestUtil::Log("Skipping: R8G8B8A8 UAV not supported");
            return;
        }

        const auto srcPixels = MakeGrayRgba({
             10,  80,
            140, 220,
        });
        auto src = CreateTexture2DFromRGBA(*fx.core, srcPixels.data(), 2, 2, D3D11_BIND_SHADER_RESOURCE);

        D3D11PyramidProcessor pyramid;
        pyramid.Initialize(fx.processing);
        auto dst = pyramid.CreateUpsampledTexture(*fx.core, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM);

        PyramidUpsampleDesc desc{};
        desc.filter = ProcessingFilter::Point;
        desc.edgeMode = PyramidEdgeMode::Clamp;
        pyramid.DispatchUpsample2x(fx.core->GetImmediateContext(), src, dst, desc);

        RequireBytesNear(
            ReadbackRgba8(*fx.core, dst),
            MakeExpectedPointUpsample2x(srcPixels, 2, 2),
            0,
            "pyramid upsample point");
    });

    TEST_RUN("Pyramid validation edge cases", {
        Fixture fx;
        if (!fx.processing.SupportsRgba8Uav()) {
            TestUtil::Log("Skipping: R8G8B8A8 UAV not supported");
            return;
        }

        const auto srcPixels = MakeGrayRgba({
            10, 20,
            30, 40,
        });
        auto src = CreateTexture2DFromRGBA(*fx.core, srcPixels.data(), 2, 2, D3D11_BIND_SHADER_RESOURCE);

        D3D11PyramidProcessor pyramid;
        pyramid.Initialize(fx.processing);
        auto badSizeDst = pyramid.CreateOutputTexture(*fx.core, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM);

        PyramidDownsampleDesc down{};
        bool downThrew = false;
        try {
            pyramid.DispatchDownsample2x(fx.core->GetImmediateContext(), src, badSizeDst, down);
        } catch (...) {
            downThrew = true;
        }
        if (!downThrew) {
            throw std::runtime_error("downsample bad dst size did not throw");
        }

        auto smallDst = pyramid.CreateOutputTexture(*fx.core, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM);
        PyramidUpsampleDesc up{};
        up.filter = ProcessingFilter::Linear;
        bool upThrew = false;
        try {
            pyramid.DispatchUpsample2x(fx.core->GetImmediateContext(), src, smallDst, up);
        } catch (...) {
            upThrew = true;
        }
        if (!upThrew) {
            throw std::runtime_error("upsample bad dst size did not throw");
        }
    });

    return TestUtil::Result("ProcessingPyramidGolden");
}
