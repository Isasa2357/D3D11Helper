//
// test_yuv_hlsl_primitives.cpp
//
#include "TestUtil.hpp"

#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>
#include <D3D11Helper/D3D11Processing/D3D11Processing.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace D3D11CoreLib;
using namespace D3D11CoreLib::Processing;

namespace {

struct Float3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

float Clamp(float value, float lo, float hi) {
    return std::max(lo, std::min(value, hi));
}

float Clamp01(float value) {
    return Clamp(value, 0.0f, 1.0f);
}

float RoundCode(float value) {
    return std::floor(value + 0.5f);
}

float CodeMaximum(DXGI_FORMAT format) {
    return format == DXGI_FORMAT_P010 ? 1023.0f : 255.0f;
}

float EightBitScale(DXGI_FORMAT format) {
    return format == DXGI_FORMAT_P010 ? 4.0f : 1.0f;
}

float ChromaCenter(DXGI_FORMAT format) {
    return 128.0f * EightBitScale(format);
}

float CodeToSample(float code, DXGI_FORMAT format) {
    code = Clamp(RoundCode(code), 0.0f, CodeMaximum(format));
    return format == DXGI_FORMAT_P010
        ? code * 64.0f / 65535.0f
        : code / 255.0f;
}

float SampleToCode(float sample, DXGI_FORMAT format) {
    sample = Clamp01(sample);
    if (format == DXGI_FORMAT_P010) {
        return Clamp(RoundCode(sample * 65535.0f / 64.0f), 0.0f, 1023.0f);
    }
    return Clamp(RoundCode(sample * 255.0f), 0.0f, 255.0f);
}

Float3 RgbToYuvSignal(Float3 rgb, ProcessingColorMatrix matrix) {
    rgb.x = Clamp01(rgb.x);
    rgb.y = Clamp01(rgb.y);
    rgb.z = Clamp01(rgb.z);

    Float3 result;
    if (matrix == ProcessingColorMatrix::BT601) {
        result.x = 0.299000f * rgb.x + 0.587000f * rgb.y + 0.114000f * rgb.z;
        result.y = (rgb.z - result.x) / 1.772000f;
        result.z = (rgb.x - result.x) / 1.402000f;
    } else if (matrix == ProcessingColorMatrix::BT2020) {
        result.x = 0.262700f * rgb.x + 0.678000f * rgb.y + 0.059300f * rgb.z;
        result.y = (rgb.z - result.x) / 1.881400f;
        result.z = (rgb.x - result.x) / 1.474600f;
    } else {
        result.x = 0.212600f * rgb.x + 0.715200f * rgb.y + 0.072200f * rgb.z;
        result.y = (rgb.z - result.x) / 1.855600f;
        result.z = (rgb.x - result.x) / 1.574800f;
    }
    return result;
}

Float3 YuvToRgbSignal(Float3 signal, ProcessingColorMatrix matrix) {
    Float3 result;
    if (matrix == ProcessingColorMatrix::BT601) {
        result.x = signal.x + 1.402000f * signal.z;
        result.y = signal.x - 0.344136f * signal.y - 0.714136f * signal.z;
        result.z = signal.x + 1.772000f * signal.y;
    } else if (matrix == ProcessingColorMatrix::BT2020) {
        result.x = signal.x + 1.474600f * signal.z;
        result.y = signal.x - 0.164553f * signal.y - 0.571353f * signal.z;
        result.z = signal.x + 1.881400f * signal.y;
    } else {
        result.x = signal.x + 1.574800f * signal.z;
        result.y = signal.x - 0.187324f * signal.y - 0.468124f * signal.z;
        result.z = signal.x + 1.855600f * signal.y;
    }
    result.x = Clamp01(result.x);
    result.y = Clamp01(result.y);
    result.z = Clamp01(result.z);
    return result;
}

Float3 EncodeCode(
    Float3 rgb,
    DXGI_FORMAT format,
    ProcessingColorRange range,
    ProcessingColorMatrix matrix) {

    const Float3 signal = RgbToYuvSignal(rgb, matrix);
    const float scale = EightBitScale(format);
    const float maximum = CodeMaximum(format);

    Float3 code;
    if (range == ProcessingColorRange::Limited) {
        code.x = Clamp(16.0f * scale + 219.0f * scale * signal.x,
            16.0f * scale, 235.0f * scale);
        code.y = Clamp(128.0f * scale + 224.0f * scale * signal.y,
            16.0f * scale, 240.0f * scale);
        code.z = Clamp(128.0f * scale + 224.0f * scale * signal.z,
            16.0f * scale, 240.0f * scale);
    } else {
        code.x = Clamp(maximum * signal.x, 0.0f, maximum);
        code.y = Clamp(ChromaCenter(format) + maximum * signal.y, 0.0f, maximum);
        code.z = Clamp(ChromaCenter(format) + maximum * signal.z, 0.0f, maximum);
    }

    code.x = RoundCode(code.x);
    code.y = RoundCode(code.y);
    code.z = RoundCode(code.z);
    return code;
}

Float3 DecodeCode(
    Float3 code,
    DXGI_FORMAT format,
    ProcessingColorRange range,
    ProcessingColorMatrix matrix) {

    const float scale = EightBitScale(format);
    const float maximum = CodeMaximum(format);

    Float3 signal;
    if (range == ProcessingColorRange::Limited) {
        signal.x = (code.x - 16.0f * scale) / (219.0f * scale);
        signal.y = (code.y - 128.0f * scale) / (224.0f * scale);
        signal.z = (code.z - 128.0f * scale) / (224.0f * scale);
    } else {
        signal.x = code.x / maximum;
        signal.y = (code.y - ChromaCenter(format)) / maximum;
        signal.z = (code.z - ChromaCenter(format)) / maximum;
    }
    return YuvToRgbSignal(signal, matrix);
}

uint8_t ToByte(float value) {
    return static_cast<uint8_t>(Clamp(RoundCode(Clamp01(value) * 255.0f), 0.0f, 255.0f));
}

void Require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void RequireCode(Float3 actual, Float3 expected, const char* label) {
    if (actual.x != expected.x || actual.y != expected.y || actual.z != expected.z) {
        std::ostringstream os;
        os << label << ": actual=(" << actual.x << ", " << actual.y << ", " << actual.z
           << ") expected=(" << expected.x << ", " << expected.y << ", " << expected.z << ")";
        throw std::runtime_error(os.str());
    }
}

void RequireRgbNear(Float3 actual, Float3 expected, float epsilon, const char* label) {
    if (std::fabs(actual.x - expected.x) > epsilon ||
        std::fabs(actual.y - expected.y) > epsilon ||
        std::fabs(actual.z - expected.z) > epsilon) {
        std::ostringstream os;
        os << label << ": actual=(" << actual.x << ", " << actual.y << ", " << actual.z
           << ") expected=(" << expected.x << ", " << expected.y << ", " << expected.z
           << ") epsilon=" << epsilon;
        throw std::runtime_error(os.str());
    }
}

bool HasPrimitiveShader(const std::filesystem::path& directory) {
    std::error_code ec;
    return std::filesystem::exists(directory / "YuvPrimitives.hlsli", ec) && !ec &&
           std::filesystem::exists(directory / "YuvPrimitiveProbe.hlsl", ec) && !ec;
}

std::filesystem::path ProcessingShaderDirectory() {
    const auto namespacedRuntime =
        std::filesystem::current_path() / "D3D11Helper" / "shaders" / "D3D11Processing";
    if (HasPrimitiveShader(namespacedRuntime)) {
        return namespacedRuntime;
    }

    const auto legacyRuntime =
        std::filesystem::current_path() / "shaders" / "D3D11Processing";
    if (HasPrimitiveShader(legacyRuntime)) {
        return legacyRuntime;
    }

#ifdef D3D11HELPER_TEST_SOURCE_DIR
    const auto source = std::filesystem::u8path(D3D11HELPER_TEST_SOURCE_DIR)
        .parent_path() / "shaders" / "D3D11Processing";
    if (HasPrimitiveShader(source)) {
        return source;
    }
#endif

    return namespacedRuntime;
}

struct Fixture {
    std::shared_ptr<D3D11Core> core;
    D3D11ProcessingContext processing;

    Fixture()
        : core(TestUtil::MakeCore()) {
        processing.Initialize(*core, ProcessingShaderDirectory());
    }
};

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
    D3D11CORE_THROW_IF_FAILED(core.GetImmediateContext()->Map(
        staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));

    std::vector<uint8_t> bytes(static_cast<size_t>(desc.Width) * desc.Height * 4u);
    for (UINT row = 0; row < desc.Height; ++row) {
        std::memcpy(
            bytes.data() + static_cast<size_t>(row) * desc.Width * 4u,
            static_cast<const uint8_t*>(mapped.pData) + static_cast<size_t>(row) * mapped.RowPitch,
            static_cast<size_t>(desc.Width) * 4u);
    }

    core.GetImmediateContext()->Unmap(staging.Get(), 0);
    return bytes;
}

std::vector<uint8_t> ExecuteProbe(Fixture& fixture) {
    D3D11ProcessingShaderCache cache;
    cache.Initialize(fixture.processing);

    D3D11ComputePipeline pipeline;
    pipeline.Initialize(
        fixture.core->GetDevice(),
        cache.GetComputeShader("YuvPrimitiveProbe.hlsl"));

    auto output = CreateTexture2D(
        *fixture.core,
        8,
        1,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        D3D11_BIND_UNORDERED_ACCESS);
    auto views = CreateRgbaTextureViewSet(
        fixture.processing,
        output,
        false,
        true,
        DXGI_FORMAT_R8G8B8A8_UNORM);

    auto* context = fixture.core->GetImmediateContext();
    ID3D11UnorderedAccessView* uav = views.uav.Get();
    const UINT initialCount = static_cast<UINT>(-1);
    context->CSSetUnorderedAccessViews(0, 1, &uav, &initialCount);
    pipeline.Dispatch(context, 1, 1, 1);

    ID3D11UnorderedAccessView* nullUav = nullptr;
    context->CSSetUnorderedAccessViews(0, 1, &nullUav, nullptr);
    return ReadbackRgba8(*fixture.core, output);
}

std::array<Float3, 8> ProbeCodes() {
    return {{
        { 81.0f, 90.0f, 240.0f },
        { 63.0f, 102.0f, 240.0f },
        { 74.0f, 97.0f, 240.0f },
        { 16.0f, 128.0f, 128.0f },
        { 235.0f, 128.0f, 128.0f },
        { 250.0f, 409.0f, 960.0f },
        { 54.0f, 99.0f, 255.0f },
        { 217.0f, 395.0f, 1023.0f },
    }};
}

std::array<Float3, 8> ExpectedProbeRgb() {
    const auto code = ProbeCodes();
    return {{
        DecodeCode(code[0], DXGI_FORMAT_NV12, ProcessingColorRange::Limited, ProcessingColorMatrix::BT601),
        DecodeCode(code[1], DXGI_FORMAT_NV12, ProcessingColorRange::Limited, ProcessingColorMatrix::BT709),
        DecodeCode(code[2], DXGI_FORMAT_NV12, ProcessingColorRange::Limited, ProcessingColorMatrix::BT2020),
        DecodeCode(code[3], DXGI_FORMAT_NV12, ProcessingColorRange::Limited, ProcessingColorMatrix::BT709),
        DecodeCode(code[4], DXGI_FORMAT_NV12, ProcessingColorRange::Limited, ProcessingColorMatrix::BT709),
        DecodeCode(code[5], DXGI_FORMAT_P010, ProcessingColorRange::Limited, ProcessingColorMatrix::BT709),
        DecodeCode(code[6], DXGI_FORMAT_NV12, ProcessingColorRange::Full, ProcessingColorMatrix::BT709),
        DecodeCode(code[7], DXGI_FORMAT_P010, ProcessingColorRange::Full, ProcessingColorMatrix::BT709),
    }};
}

struct PlanarReadback {
    D3D11_MAPPED_SUBRESOURCE mapped{};
    ComPtr<ID3D11Texture2D> staging;
    ID3D11DeviceContext* context = nullptr;
    UINT width = 0;
    UINT height = 0;

    PlanarReadback() = default;
    PlanarReadback(const PlanarReadback&) = delete;
    PlanarReadback& operator=(const PlanarReadback&) = delete;

    PlanarReadback(PlanarReadback&& other) noexcept
        : mapped(other.mapped)
        , staging(std::move(other.staging))
        , context(other.context)
        , width(other.width)
        , height(other.height) {
        other.mapped = {};
        other.context = nullptr;
        other.width = 0;
        other.height = 0;
    }

    PlanarReadback& operator=(PlanarReadback&& other) noexcept {
        if (this != &other) {
            Reset();
            mapped = other.mapped;
            staging = std::move(other.staging);
            context = other.context;
            width = other.width;
            height = other.height;
            other.mapped = {};
            other.context = nullptr;
            other.width = 0;
            other.height = 0;
        }
        return *this;
    }

    ~PlanarReadback() {
        Reset();
    }

    void Reset() noexcept {
        if (context && staging && mapped.pData) {
            context->Unmap(staging.Get(), 0);
        }
        mapped = {};
        staging.Reset();
        context = nullptr;
        width = 0;
        height = 0;
    }

    const uint8_t* Bytes() const noexcept {
        return static_cast<const uint8_t*>(mapped.pData);
    }
};

PlanarReadback MapPlanarReadback(D3D11Core& core, D3D11Resource& texture) {
    const auto desc = texture.GetTexture2DDesc();
    D3D11_TEXTURE2D_DESC stagingDesc = desc;
    stagingDesc.BindFlags = 0;
    stagingDesc.MiscFlags = 0;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    PlanarReadback result;
    result.context = core.GetImmediateContext();
    result.width = desc.Width;
    result.height = desc.Height;
    D3D11CORE_THROW_IF_FAILED(core.GetDevice()->CreateTexture2D(
        &stagingDesc, nullptr, &result.staging));
    result.context->CopyResource(result.staging.Get(), texture.Get());
    D3D11CORE_THROW_IF_FAILED(result.context->Map(
        result.staging.Get(), 0, D3D11_MAP_READ, 0, &result.mapped));
    return result;
}

uint16_t ReadU16(const uint8_t* data) {
    uint16_t value = 0;
    std::memcpy(&value, data, sizeof(value));
    return value;
}

D3D11Resource ConvertUniformRed(Fixture& fixture, DXGI_FORMAT destinationFormat) {
    const std::vector<uint8_t> redRgba = {
        255, 0, 0, 255, 255, 0, 0, 255,
        255, 0, 0, 255, 255, 0, 0, 255,
    };

    auto source = CreateTexture2DFromRGBA(
        *fixture.core,
        redRgba.data(),
        2,
        2,
        D3D11_BIND_SHADER_RESOURCE);

    D3D11FormatConverter converter;
    converter.Initialize(fixture.processing);
    auto destination = converter.CreateOutputTexture(
        *fixture.core,
        2,
        2,
        destinationFormat);

    FormatConvertDesc desc{};
    desc.srcFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.dstFormat = destinationFormat;
    desc.color.dstMatrix = ProcessingColorMatrix::BT709;
    desc.color.dstRange = ProcessingColorRange::Limited;
    converter.DispatchConvert(
        fixture.core->GetImmediateContext(),
        source,
        destination,
        desc);
    return destination;
}

} // namespace

int main() {
    TEST_RUN("YUV known limited-range red codes", {
        const Float3 red = { 1.0f, 0.0f, 0.0f };

        RequireCode(
            EncodeCode(red, DXGI_FORMAT_NV12, ProcessingColorRange::Limited, ProcessingColorMatrix::BT601),
            { 81.0f, 90.0f, 240.0f },
            "BT.601 NV12 limited red");
        RequireCode(
            EncodeCode(red, DXGI_FORMAT_NV12, ProcessingColorRange::Limited, ProcessingColorMatrix::BT709),
            { 63.0f, 102.0f, 240.0f },
            "BT.709 NV12 limited red");
        RequireCode(
            EncodeCode(red, DXGI_FORMAT_NV12, ProcessingColorRange::Limited, ProcessingColorMatrix::BT2020),
            { 74.0f, 97.0f, 240.0f },
            "BT.2020 NV12 limited red");

        RequireCode(
            EncodeCode(red, DXGI_FORMAT_P010, ProcessingColorRange::Limited, ProcessingColorMatrix::BT601),
            { 326.0f, 361.0f, 960.0f },
            "BT.601 P010 limited red");
        RequireCode(
            EncodeCode(red, DXGI_FORMAT_P010, ProcessingColorRange::Limited, ProcessingColorMatrix::BT709),
            { 250.0f, 409.0f, 960.0f },
            "BT.709 P010 limited red");
        RequireCode(
            EncodeCode(red, DXGI_FORMAT_P010, ProcessingColorRange::Limited, ProcessingColorMatrix::BT2020),
            { 294.0f, 387.0f, 960.0f },
            "BT.2020 P010 limited red");
    });

    TEST_RUN("P010 high-bit storage round trip", {
        const std::array<float, 8> codes = {
            0.0f, 1.0f, 64.0f, 250.0f, 512.0f, 940.0f, 960.0f, 1023.0f
        };
        for (float code : codes) {
            Require(
                SampleToCode(CodeToSample(code, DXGI_FORMAT_P010), DXGI_FORMAT_P010) == code,
                "P010 code/sample round trip mismatch");
        }
    });

    TEST_RUN("CPU reference round trips matrices ranges and formats", {
        const std::array<Float3, 5> colors = {{
            { 0.0f, 0.0f, 0.0f },
            { 1.0f, 1.0f, 1.0f },
            { 1.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f },
            { 0.1f, 0.4f, 0.8f },
        }};
        const std::array<ProcessingColorMatrix, 3> matrices = {
            ProcessingColorMatrix::BT601,
            ProcessingColorMatrix::BT709,
            ProcessingColorMatrix::BT2020,
        };
        const std::array<ProcessingColorRange, 2> ranges = {
            ProcessingColorRange::Full,
            ProcessingColorRange::Limited,
        };
        const std::array<DXGI_FORMAT, 2> formats = {
            DXGI_FORMAT_NV12,
            DXGI_FORMAT_P010,
        };

        for (DXGI_FORMAT format : formats) {
            const float epsilon = format == DXGI_FORMAT_P010 ? 0.0040f : 0.0120f;
            for (ProcessingColorRange range : ranges) {
                for (ProcessingColorMatrix matrix : matrices) {
                    for (const auto& color : colors) {
                        const auto decoded = DecodeCode(
                            EncodeCode(color, format, range, matrix),
                            format,
                            range,
                            matrix);
                        RequireRgbNear(decoded, color, epsilon, "CPU YUV round trip");
                    }
                }
            }
        }
    });

    TEST_RUN("Refactored shaders compile with YUV primitive library", {
        Fixture fixture;
        D3D11ProcessingShaderCache cache;
        cache.Initialize(fixture.processing);

        Require(!cache.GetComputeShader("YuvPrimitiveProbe.hlsl").Empty(), "probe shader is empty");
        Require(!cache.GetComputeShader("ConvertYuv420ToRgb.hlsl").Empty(), "YUV->RGB shader is empty");
        Require(!cache.GetComputeShader("ConvertRgbToYuv420.hlsl").Empty(), "RGB->YUV shader is empty");
        Require(!cache.GetComputeShader("FusedYuv420ToRgbResize.hlsl").Empty(), "fused YUV resize shader is empty");
        Require(!cache.GetComputeShader("ResizeRgba.hlsl").Empty(), "RGBA resize shader is empty");
        Require(!cache.GetComputeShader("FusedRgbToRgbResize.hlsl").Empty(), "fused RGBA resize shader is empty");
    });

    TEST_RUN("GPU probe matches CPU golden values", {
        Fixture fixture;
        if (!fixture.processing.SupportsRgba8Uav()) {
            TestUtil::Log("Skipping: R8G8B8A8 UAV is not supported");
            return;
        }

        const auto actual = ExecuteProbe(fixture);
        const auto expectedRgb = ExpectedProbeRgb();
        Require(actual.size() == expectedRgb.size() * 4u, "GPU probe output size mismatch");

        for (size_t i = 0; i < expectedRgb.size(); ++i) {
            const uint8_t expected[4] = {
                ToByte(expectedRgb[i].x),
                ToByte(expectedRgb[i].y),
                ToByte(expectedRgb[i].z),
                255u,
            };
            for (size_t channel = 0; channel < 4; ++channel) {
                const int difference = std::abs(
                    static_cast<int>(actual[i * 4u + channel]) -
                    static_cast<int>(expected[channel]));
                if (difference > 2) {
                    std::ostringstream os;
                    os << "GPU golden mismatch at pixel=" << i
                       << " channel=" << channel
                       << " actual=" << static_cast<int>(actual[i * 4u + channel])
                       << " expected=" << static_cast<int>(expected[channel]);
                    throw std::runtime_error(os.str());
                }
            }
        }
    });

    TEST_RUN("NV12 store writes BT.709 limited codes", {
        Fixture fixture;
        if (!fixture.processing.SupportsNv12Uav()) {
            TestUtil::Log("Skipping: NV12 UAV plane views are not supported");
            return;
        }

        auto texture = ConvertUniformRed(fixture, DXGI_FORMAT_NV12);
        auto readback = MapPlanarReadback(*fixture.core, texture);
        const auto* base = readback.Bytes();

        for (UINT row = 0; row < 2; ++row) {
            const auto* rowData = base + static_cast<size_t>(row) * readback.mapped.RowPitch;
            Require(rowData[0] == 63u && rowData[1] == 63u, "NV12 luma code mismatch");
        }

        const auto* uv = base + static_cast<size_t>(readback.mapped.RowPitch) * readback.height;
        Require(uv[0] == 102u && uv[1] == 240u, "NV12 chroma code mismatch");
    });

    TEST_RUN("P010 store writes high ten-bit codes", {
        Fixture fixture;
        if (!fixture.processing.SupportsP010Uav()) {
            TestUtil::Log("Skipping: P010 UAV plane views are not supported");
            return;
        }

        auto texture = ConvertUniformRed(fixture, DXGI_FORMAT_P010);
        auto readback = MapPlanarReadback(*fixture.core, texture);
        const auto* base = readback.Bytes();

        for (UINT row = 0; row < 2; ++row) {
            const auto* rowData = base + static_cast<size_t>(row) * readback.mapped.RowPitch;
            Require(
                ReadU16(rowData + 0) == static_cast<uint16_t>(250u << 6u) &&
                ReadU16(rowData + 2) == static_cast<uint16_t>(250u << 6u),
                "P010 luma storage mismatch");
        }

        const auto* uv = base + static_cast<size_t>(readback.mapped.RowPitch) * readback.height;
        Require(
            ReadU16(uv + 0) == static_cast<uint16_t>(409u << 6u) &&
            ReadU16(uv + 2) == static_cast<uint16_t>(960u << 6u),
            "P010 chroma storage mismatch");
    });

    return TestUtil::Result("D3D11 YUV HLSL primitives");
}
