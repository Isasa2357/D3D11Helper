//
// test_processing_golden.cpp - Processing golden pixel regression tests.
//
#include "TestUtil.hpp"

#include <D3D11Helper/D3D11Processing/D3D11Processing.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
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
    if (std::filesystem::exists(runtimeDir / "ConvertRgbToRgb.hlsl")) {
        return runtimeDir;
    }
#ifdef D3D11HELPER_TEST_SOURCE_DIR
    const auto sourceDir = std::filesystem::u8path(D3D11HELPER_TEST_SOURCE_DIR).parent_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(sourceDir / "ConvertRgbToRgb.hlsl")) {
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

void Require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

uint8_t ClampByte(int v) {
    return static_cast<uint8_t>(std::max(0, std::min(255, v)));
}

uint8_t FloatToByte(float v) {
    v = std::max(0.0f, std::min(1.0f, v));
    return ClampByte(static_cast<int>(std::lround(v * 255.0f)));
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

void RequireBytesEqual(const std::vector<uint8_t>& actual, const std::vector<uint8_t>& expected, const char* label) {
    Require(actual.size() == expected.size(), std::string(label) + ": size mismatch");
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

void RequireBytesNear(const std::vector<uint8_t>& actual, const std::vector<uint8_t>& expected, uint8_t tolerance, const char* label) {
    Require(actual.size() == expected.size(), std::string(label) + ": size mismatch");
    for (size_t i = 0; i < expected.size(); ++i) {
        RequireByteNear(actual[i], expected[i], tolerance, label, i);
    }
}

std::vector<uint8_t> ReadbackRgba8(D3D11Core& core, D3D11Resource& texture) {
    const auto desc = texture.GetTexture2DDesc();
    D3D11_TEXTURE2D_DESC stagingDesc = desc;
    stagingDesc.BindFlags = 0;
    stagingDesc.MiscFlags = 0;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    ComPtr<ID3D11Texture2D> staging;
    D3D11CORE_THROW_IF_FAILED(core.GetDevice()->CreateTexture2D(&stagingDesc, nullptr, &staging));
    core.GetImmediateContext()->CopyResource(staging.Get(), texture.Get());

    D3D11_MAPPED_SUBRESOURCE mapped{};
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

struct Nv12Readback {
    std::vector<uint8_t> y;
    std::vector<uint8_t> uv;
};

Nv12Readback ReadbackNv12(D3D11Core& core, D3D11Resource& texture) {
    const auto desc = texture.GetTexture2DDesc();
    D3D11_TEXTURE2D_DESC stagingDesc = desc;
    stagingDesc.BindFlags = 0;
    stagingDesc.MiscFlags = 0;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    ComPtr<ID3D11Texture2D> staging;
    D3D11CORE_THROW_IF_FAILED(core.GetDevice()->CreateTexture2D(&stagingDesc, nullptr, &staging));
    core.GetImmediateContext()->CopyResource(staging.Get(), texture.Get());

    D3D11_MAPPED_SUBRESOURCE mapped{};
    D3D11CORE_THROW_IF_FAILED(core.GetImmediateContext()->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));

    Nv12Readback out;
    out.y.resize(static_cast<size_t>(desc.Width) * desc.Height);
    out.uv.resize(static_cast<size_t>(desc.Width) * (desc.Height / 2));

    const auto* base = static_cast<const uint8_t*>(mapped.pData);
    for (UINT row = 0; row < desc.Height; ++row) {
        std::memcpy(out.y.data() + static_cast<size_t>(row) * desc.Width,
                    base + static_cast<size_t>(row) * mapped.RowPitch,
                    desc.Width);
    }

    const auto* uvBase = base + static_cast<size_t>(mapped.RowPitch) * desc.Height;
    for (UINT row = 0; row < desc.Height / 2; ++row) {
        std::memcpy(out.uv.data() + static_cast<size_t>(row) * desc.Width,
                    uvBase + static_cast<size_t>(row) * mapped.RowPitch,
                    desc.Width);
    }

    core.GetImmediateContext()->Unmap(staging.Get(), 0);
    return out;
}

D3D11Resource CreatePlanarTexture(
    D3D11Core& core,
    UINT width,
    UINT height,
    DXGI_FORMAT format,
    const void* data,
    UINT rowPitch,
    UINT bindFlags) {

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = bindFlags;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = data;
    init.SysMemPitch = rowPitch;

    ComPtr<ID3D11Texture2D> texture;
    D3D11CORE_THROW_IF_FAILED(core.GetDevice()->CreateTexture2D(&desc, &init, &texture));
    return D3D11Resource(std::move(texture));
}

D3D11Resource CreateNv12TextureWithPlanes(
    D3D11Core& core,
    UINT width,
    UINT height,
    const std::vector<uint8_t>& yPlane,
    const std::vector<uint8_t>& uvPlane,
    UINT bindFlags = D3D11_BIND_SHADER_RESOURCE) {

    Require(width % 2u == 0 && height % 2u == 0, "NV12 test texture dimensions must be even");
    Require(yPlane.size() == static_cast<size_t>(width) * height, "NV12 Y plane size mismatch");
    Require(uvPlane.size() == static_cast<size_t>(width) * (height / 2u), "NV12 UV plane size mismatch");

    std::vector<uint8_t> data;
    data.reserve(yPlane.size() + uvPlane.size());
    data.insert(data.end(), yPlane.begin(), yPlane.end());
    data.insert(data.end(), uvPlane.begin(), uvPlane.end());

    return CreatePlanarTexture(core, width, height, DXGI_FORMAT_NV12, data.data(), width, bindFlags);
}

D3D11Resource CreateSolidNv12Texture(
    D3D11Core& core,
    UINT width,
    UINT height,
    uint8_t yValue,
    uint8_t uValue,
    uint8_t vValue,
    UINT bindFlags = D3D11_BIND_SHADER_RESOURCE) {

    std::vector<uint8_t> yPlane(static_cast<size_t>(width) * height, yValue);
    std::vector<uint8_t> uvPlane(static_cast<size_t>(width) * (height / 2u));
    for (size_t i = 0; i < uvPlane.size(); i += 2) {
        uvPlane[i + 0] = uValue;
        uvPlane[i + 1] = vValue;
    }
    return CreateNv12TextureWithPlanes(core, width, height, yPlane, uvPlane, bindFlags);
}

D3D11Resource CreateSolidP010Texture(
    D3D11Core& core,
    UINT width,
    UINT height,
    uint16_t yValue,
    uint16_t uvValue,
    UINT bindFlags = D3D11_BIND_SHADER_RESOURCE) {

    std::vector<uint16_t> data(static_cast<size_t>(width) * height + static_cast<size_t>(width) * (height / 2u));
    std::fill(data.begin(), data.begin() + static_cast<size_t>(width) * height, yValue);
    std::fill(data.begin() + static_cast<size_t>(width) * height, data.end(), uvValue);
    return CreatePlanarTexture(core, width, height, DXGI_FORMAT_P010, data.data(), width * sizeof(uint16_t), bindFlags);
}

std::vector<uint8_t> MakePointResizeExpected2x2To4x4(const std::vector<uint8_t>& src) {
    std::vector<uint8_t> expected(4u * 4u * 4u);
    for (UINT y = 0; y < 4; ++y) {
        for (UINT x = 0; x < 4; ++x) {
            const UINT sx = x / 2u;
            const UINT sy = y / 2u;
            std::memcpy(expected.data() + static_cast<size_t>(y * 4u + x) * 4u,
                        src.data() + static_cast<size_t>(sy * 2u + sx) * 4u,
                        4u);
        }
    }
    return expected;
}

std::vector<uint8_t> ExpectedNv12FullBt709(uint8_t yByte, uint8_t uByte, uint8_t vByte, UINT width, UINT height) {
    const float y = float(yByte) / 255.0f;
    const float u = float(uByte) / 255.0f - 0.5f;
    const float v = float(vByte) / 255.0f - 0.5f;

    const float r = y + 1.574800f * v;
    const float g = y - 0.187324f * u - 0.468124f * v;
    const float b = y + 1.855600f * u;

    const uint8_t rb = FloatToByte(r);
    const uint8_t gb = FloatToByte(g);
    const uint8_t bb = FloatToByte(b);

    std::vector<uint8_t> expected(static_cast<size_t>(width) * height * 4u);
    for (size_t i = 0; i < expected.size(); i += 4) {
        expected[i + 0] = rb;
        expected[i + 1] = gb;
        expected[i + 2] = bb;
        expected[i + 3] = 255;
    }
    return expected;
}

void RequireAllRgbaNear(const std::vector<uint8_t>& actual, const std::vector<uint8_t>& expectedPixel, uint8_t tolerance, const char* label) {
    Require(expectedPixel.size() == 4, "expected pixel must have 4 channels");
    Require(actual.size() % 4 == 0, std::string(label) + ": output is not RGBA byte aligned");
    for (size_t i = 0; i < actual.size(); i += 4) {
        for (size_t c = 0; c < 4; ++c) {
            RequireByteNear(actual[i + c], expectedPixel[c], tolerance, label, i + c);
        }
    }
}

} // namespace

int main() {
    TEST_RUN("Processing golden RGBA copy and point resize", {
        Fixture fx;
        if (!fx.processing.SupportsRgba8Uav()) {
            TestUtil::Log("Skipping: R8G8B8A8 UAV not supported");
            return;
        }

        const std::vector<uint8_t> srcPixels = {
             10,  20,  30, 255,   40,  50,  60, 255,
             70,  80,  90, 255,  100, 110, 120, 255,
        };
        auto src = CreateTexture2DFromRGBA(*fx.core, srcPixels.data(), 2, 2, D3D11_BIND_SHADER_RESOURCE);

        D3D11FormatConverter converter;
        converter.Initialize(fx.processing);
        auto copy = converter.CreateOutputTexture(*fx.core, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM);
        FormatConvertDesc convertDesc{};
        convertDesc.srcFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        convertDesc.dstFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        converter.DispatchConvert(fx.core->GetImmediateContext(), src, copy, convertDesc);
        RequireBytesEqual(ReadbackRgba8(*fx.core, copy), srcPixels, "rgba copy");

        D3D11Resizer resizer;
        resizer.Initialize(fx.processing);
        auto resized = resizer.CreateOutputTexture(*fx.core, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM);
        ResizeDesc resizeDesc{};
        resizeDesc.filter = ProcessingFilter::Point;
        resizer.DispatchResize(fx.core->GetImmediateContext(), src, resized, resizeDesc);
        RequireBytesEqual(ReadbackRgba8(*fx.core, resized), MakePointResizeExpected2x2To4x4(srcPixels), "rgba point resize 2x2->4x4");
    });

    TEST_RUN("Processing golden RGBA linear resize center", {
        Fixture fx;
        if (!fx.processing.SupportsRgba8Uav()) {
            TestUtil::Log("Skipping: R8G8B8A8 UAV not supported");
            return;
        }

        const std::vector<uint8_t> srcPixels = {
              0,   0,   0, 255,  100,   0,   0, 255,
              0, 100,   0, 255,  100, 100,   0, 255,
        };
        auto src = CreateTexture2DFromRGBA(*fx.core, srcPixels.data(), 2, 2, D3D11_BIND_SHADER_RESOURCE);
        D3D11Resizer resizer;
        resizer.Initialize(fx.processing);
        auto dst = resizer.CreateOutputTexture(*fx.core, 3, 3, DXGI_FORMAT_R8G8B8A8_UNORM);
        ResizeDesc desc{};
        desc.filter = ProcessingFilter::Linear;
        resizer.DispatchResize(fx.core->GetImmediateContext(), src, dst, desc);

        const auto got = ReadbackRgba8(*fx.core, dst);
        const size_t center = static_cast<size_t>(1u * 3u + 1u) * 4u;
        RequireByteNear(got[center + 0], 50, 2, "linear center r", center + 0);
        RequireByteNear(got[center + 1], 50, 2, "linear center g", center + 1);
        RequireByteNear(got[center + 2], 0, 1, "linear center b", center + 2);
        RequireByteNear(got[center + 3], 255, 1, "linear center a", center + 3);
    });

    TEST_RUN("Processing golden NV12 gray and color", {
        Fixture fx;
        if (!fx.processing.SupportsNv12Srv() || !fx.processing.SupportsRgba8Uav()) {
            TestUtil::Log("Skipping: NV12 SRV or R8G8B8A8 UAV not supported");
            return;
        }

        D3D11FormatConverter converter;
        converter.Initialize(fx.processing);

        auto gray = CreateSolidNv12Texture(*fx.core, 4, 4, 128, 128, 128);
        auto grayOut = converter.CreateOutputTexture(*fx.core, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM);
        FormatConvertDesc grayDesc{};
        grayDesc.srcFormat = DXGI_FORMAT_NV12;
        grayDesc.dstFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        grayDesc.color.srcRange = ProcessingColorRange::Full;
        grayDesc.color.srcMatrix = ProcessingColorMatrix::BT709;
        converter.DispatchConvert(fx.core->GetImmediateContext(), gray, grayOut, grayDesc);
        RequireAllRgbaNear(ReadbackRgba8(*fx.core, grayOut), { 128, 128, 128, 255 }, 3, "nv12 gray");

        constexpr uint8_t y = 128;
        constexpr uint8_t u = 90;
        constexpr uint8_t v = 240;
        auto color = CreateSolidNv12Texture(*fx.core, 4, 4, y, u, v);
        auto colorOut = converter.CreateOutputTexture(*fx.core, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM);
        FormatConvertDesc colorDesc{};
        colorDesc.srcFormat = DXGI_FORMAT_NV12;
        colorDesc.dstFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        colorDesc.color.srcRange = ProcessingColorRange::Full;
        colorDesc.color.srcMatrix = ProcessingColorMatrix::BT709;
        converter.DispatchConvert(fx.core->GetImmediateContext(), color, colorOut, colorDesc);
        RequireBytesNear(ReadbackRgba8(*fx.core, colorOut), ExpectedNv12FullBt709(y, u, v, 4, 4), 3, "nv12 color bt709 full");
    });

    TEST_RUN("Processing golden P010 neutral gray", {
        Fixture fx;
        if (!fx.processing.SupportsP010Srv() || !fx.processing.SupportsRgba8Uav()) {
            TestUtil::Log("Skipping: P010 SRV or R8G8B8A8 UAV not supported");
            return;
        }

        auto p010 = CreateSolidP010Texture(*fx.core, 4, 4, 32768, 32768);
        D3D11FormatConverter converter;
        converter.Initialize(fx.processing);
        auto out = converter.CreateOutputTexture(*fx.core, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM);
        FormatConvertDesc desc{};
        desc.srcFormat = DXGI_FORMAT_P010;
        desc.dstFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.color.srcRange = ProcessingColorRange::Full;
        desc.color.srcMatrix = ProcessingColorMatrix::BT709;
        converter.DispatchConvert(fx.core->GetImmediateContext(), p010, out, desc);
        RequireAllRgbaNear(ReadbackRgba8(*fx.core, out), { 128, 128, 128, 255 }, 4, "p010 gray");
    });

    TEST_RUN("Processing golden RGBA to NV12 black and white", {
        Fixture fx;
        if (!fx.processing.SupportsNv12Uav() || !fx.processing.SupportsRgba8Uav()) {
            TestUtil::Log("Skipping: NV12 UAV or R8G8B8A8 UAV not supported");
            return;
        }

        D3D11FormatConverter converter;
        converter.Initialize(fx.processing);

        const auto checkSolid = [&](uint8_t rgbaValue, uint8_t expectedY, const char* label) {
            std::vector<uint8_t> rgba(4u * 4u * 4u, rgbaValue);
            for (size_t i = 3; i < rgba.size(); i += 4) {
                rgba[i] = 255;
            }
            auto src = CreateTexture2DFromRGBA(*fx.core, rgba.data(), 4, 4, D3D11_BIND_SHADER_RESOURCE);
            auto nv12 = converter.CreateOutputTexture(*fx.core, 4, 4, DXGI_FORMAT_NV12);
            FormatConvertDesc desc{};
            desc.srcFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.dstFormat = DXGI_FORMAT_NV12;
            desc.color.dstRange = ProcessingColorRange::Full;
            desc.color.dstMatrix = ProcessingColorMatrix::BT709;
            converter.DispatchConvert(fx.core->GetImmediateContext(), src, nv12, desc);

            const auto got = ReadbackNv12(*fx.core, nv12);
            for (size_t i = 0; i < got.y.size(); ++i) {
                RequireByteNear(got.y[i], expectedY, 2, label, i);
            }
            for (size_t i = 0; i < got.uv.size(); ++i) {
                RequireByteNear(got.uv[i], 128, 2, label, got.y.size() + i);
            }
        };

        checkSolid(0, 0, "rgba black to nv12");
        checkSolid(255, 255, "rgba white to nv12");
    });

    TEST_RUN("Processing golden fused NV12 convert resize equals sequential", {
        Fixture fx;
        if (!fx.processing.SupportsNv12Srv() || !fx.processing.SupportsRgba8Uav()) {
            TestUtil::Log("Skipping: NV12 SRV or R8G8B8A8 UAV not supported");
            return;
        }

        const std::vector<uint8_t> yPlane = {
             16,  32,  48,  64,
             80,  96, 112, 128,
            144, 160, 176, 192,
            208, 224, 240, 255,
        };
        std::vector<uint8_t> uvPlane(4u * 2u, 128);
        auto nv12 = CreateNv12TextureWithPlanes(*fx.core, 4, 4, yPlane, uvPlane);

        D3D11FormatConverter converter;
        converter.Initialize(fx.processing);
        auto rgba = converter.CreateOutputTexture(*fx.core, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM);
        FormatConvertDesc convertDesc{};
        convertDesc.srcFormat = DXGI_FORMAT_NV12;
        convertDesc.dstFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        convertDesc.color.srcRange = ProcessingColorRange::Full;
        converter.DispatchConvert(fx.core->GetImmediateContext(), nv12, rgba, convertDesc);

        D3D11Resizer resizer;
        resizer.Initialize(fx.processing);
        auto sequential = resizer.CreateOutputTexture(*fx.core, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM);
        ResizeDesc resizeDesc{};
        resizeDesc.filter = ProcessingFilter::Point;
        resizer.DispatchResize(fx.core->GetImmediateContext(), rgba, sequential, resizeDesc);

        D3D11FusedProcessor fused;
        fused.Initialize(fx.processing);
        auto fusedOut = fused.CreateOutputTexture(*fx.core, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM);
        FusedConvertResizeDesc fusedDesc{};
        fusedDesc.srcFormat = DXGI_FORMAT_NV12;
        fusedDesc.dstFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        fusedDesc.color.srcRange = ProcessingColorRange::Full;
        fusedDesc.filter = ProcessingFilter::Point;
        fused.DispatchConvertResize(fx.core->GetImmediateContext(), nv12, fusedOut, fusedDesc);

        RequireBytesNear(ReadbackRgba8(*fx.core, fusedOut), ReadbackRgba8(*fx.core, sequential), 1, "fused equals sequential");
    });

    TEST_RUN("Processing golden remap identity and constant border shift", {
        Fixture fx;
        if (!fx.processing.SupportsRgba8Uav()) {
            TestUtil::Log("Skipping: R8G8B8A8 UAV not supported");
            return;
        }

        const std::vector<uint8_t> srcPixels = {
            255,   0,   0, 255,    0, 255,   0, 255,    0,   0, 255, 255,
            255, 255,   0, 255,    0, 255, 255, 255,  255,   0, 255, 255,
        };
        auto src = CreateTexture2DFromRGBA(*fx.core, srcPixels.data(), 3, 2, D3D11_BIND_SHADER_RESOURCE);

        const std::vector<float> identityMap = {
            0,0, 1,0, 2,0,
            0,1, 1,1, 2,1,
        };
        auto identityMapTex = CreateTexture2DFromMemory(
            *fx.core, identityMap.data(), 3, 2, DXGI_FORMAT_R32G32_FLOAT,
            3u * 2u * sizeof(float), D3D11_BIND_SHADER_RESOURCE);

        D3D11Remapper remapper;
        remapper.Initialize(fx.processing);
        auto identityOut = remapper.CreateOutputTexture(*fx.core, 3, 2, DXGI_FORMAT_R8G8B8A8_UNORM);
        RemapDesc identityDesc{};
        identityDesc.filter = ProcessingFilter::Point;
        identityDesc.borderMode = RemapBorderMode::Clamp;
        remapper.DispatchRemap(fx.core->GetImmediateContext(), src, identityMapTex, identityOut, identityDesc);
        RequireBytesEqual(ReadbackRgba8(*fx.core, identityOut), srcPixels, "remap identity");

        const std::vector<float> shiftMap = { 1,0, 2,0, 3,0 };
        auto shiftMapTex = CreateTexture2DFromMemory(
            *fx.core, shiftMap.data(), 3, 1, DXGI_FORMAT_R32G32_FLOAT,
            3u * 2u * sizeof(float), D3D11_BIND_SHADER_RESOURCE);
        auto shiftOut = remapper.CreateOutputTexture(*fx.core, 3, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
        RemapDesc shiftDesc{};
        shiftDesc.filter = ProcessingFilter::Point;
        shiftDesc.borderMode = RemapBorderMode::Constant;
        shiftDesc.borderColor[0] = 1.0f;
        shiftDesc.borderColor[1] = 0.0f;
        shiftDesc.borderColor[2] = 1.0f;
        shiftDesc.borderColor[3] = 1.0f;
        remapper.DispatchRemap(fx.core->GetImmediateContext(), src, shiftMapTex, shiftOut, shiftDesc);

        const std::vector<uint8_t> expectedShift = {
              0, 255,   0, 255,
              0,   0, 255, 255,
            255,   0, 255, 255,
        };
        RequireBytesEqual(ReadbackRgba8(*fx.core, shiftOut), expectedShift, "remap constant border shift");
    });

    TEST_RUN("Processing golden region effect rect boundary", {
        Fixture fx;
        if (!fx.processing.SupportsRgba8Uav()) {
            TestUtil::Log("Skipping: R8G8B8A8 UAV not supported");
            return;
        }

        std::vector<uint8_t> srcPixels(4u * 4u * 4u);
        for (size_t i = 0; i < srcPixels.size(); i += 4) {
            srcPixels[i + 0] = 200;
            srcPixels[i + 1] = 100;
            srcPixels[i + 2] = 50;
            srcPixels[i + 3] = 255;
        }
        auto src = CreateTexture2DFromRGBA(*fx.core, srcPixels.data(), 4, 4, D3D11_BIND_SHADER_RESOURCE);

        D3D11RegionEffect effect;
        effect.Initialize(fx.processing);
        auto dst = effect.CreateOutputTexture(*fx.core, 4, 4, DXGI_FORMAT_R8G8B8A8_UNORM);
        RegionEffectDesc desc{};
        desc.srcFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.dstFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.shape = RegionShape::Rect;
        desc.selection = RegionSelection::Outside;
        desc.effect = RegionEffectMode::Darken;
        desc.rectX = 1.0f;
        desc.rectY = 1.0f;
        desc.rectWidth = 2.0f;
        desc.rectHeight = 2.0f;
        desc.edgeSoftness = 0.0f;
        desc.darkenFactor = 0.5f;
        effect.DispatchRegionEffect(fx.core->GetImmediateContext(), src, dst, desc);

        std::vector<uint8_t> expected(4u * 4u * 4u);
        for (UINT y = 0; y < 4; ++y) {
            for (UINT x = 0; x < 4; ++x) {
                const bool inside = (x >= 1 && x <= 2 && y >= 1 && y <= 2);
                const size_t i = static_cast<size_t>(y * 4u + x) * 4u;
                expected[i + 0] = inside ? 200 : 100;
                expected[i + 1] = inside ? 100 : 50;
                expected[i + 2] = inside ? 50 : 25;
                expected[i + 3] = 255;
            }
        }
        RequireBytesEqual(ReadbackRgba8(*fx.core, dst), expected, "region rect outside darken boundary");
    });

    return TestUtil::Result("ProcessingGolden");
}
