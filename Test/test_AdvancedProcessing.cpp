#include "TestCommon.hpp"
#include <D3D11Helper/D3D11Processing/D3D11Processing.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

using namespace D3D11CoreLib;
using namespace D3D11CoreLib::Processing;

namespace {

std::filesystem::path ProcessingShaderDir() {
    const auto runtimeDir = std::filesystem::current_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(runtimeDir / "AdvancedTransformRgba.hlsl") &&
        std::filesystem::exists(runtimeDir / "ApplyLut3D.hlsl")) {
        return runtimeDir;
    }
#ifdef D3D11HELPER_TEST_SOURCE_DIR
    const auto sourceDir = std::filesystem::u8path(D3D11HELPER_TEST_SOURCE_DIR).parent_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(sourceDir / "AdvancedTransformRgba.hlsl") &&
        std::filesystem::exists(sourceDir / "ApplyLut3D.hlsl")) {
        return sourceDir;
    }
#endif
    return runtimeDir;
}

struct Fixture {
    std::shared_ptr<D3D11Core> core;
    D3D11ProcessingContext processing;

    Fixture() : core(Test::CreateCore()) {
        processing.Initialize(*core, ProcessingShaderDir());
    }
};

void RequireBytesEqual(const std::vector<uint8_t>& actual, const std::vector<uint8_t>& expected, const char* label) {
    CHECK_EQ(actual.size(), expected.size());
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
        std::memcpy(
            out.data() + static_cast<size_t>(y) * desc.Width * 4u,
            static_cast<const uint8_t*>(mapped.pData) + static_cast<size_t>(y) * mapped.RowPitch,
            static_cast<size_t>(desc.Width) * 4u);
    }

    core.GetImmediateContext()->Unmap(staging.Get(), 0);
    return out;
}

D3D11Resource CreateTexture3DFromRgba8(D3D11Core& core, UINT width, UINT height, UINT depth, const std::vector<uint8_t>& rgba) {
    CHECK(width > 0 && height > 0 && depth > 0);
    CHECK_EQ(rgba.size(), static_cast<size_t>(width) * height * depth * 4u);

    D3D11_TEXTURE3D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.Depth = depth;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = rgba.data();
    init.SysMemPitch = width * 4u;
    init.SysMemSlicePitch = width * height * 4u;

    ComPtr<ID3D11Texture3D> texture;
    D3D11CORE_THROW_IF_FAILED(core.GetDevice()->CreateTexture3D(&desc, &init, &texture));
    ComPtr<ID3D11Resource> resource;
    texture.As(&resource);
    return D3D11Resource(std::move(resource));
}

std::vector<uint8_t> MakeIdentity2x2x2LutRgba8() {
    std::vector<uint8_t> lut(2u * 2u * 2u * 4u);
    for (UINT z = 0; z < 2; ++z) {
        for (UINT y = 0; y < 2; ++y) {
            for (UINT x = 0; x < 2; ++x) {
                const size_t i = static_cast<size_t>((z * 4u + y * 2u + x) * 4u);
                lut[i + 0] = x ? 255 : 0;
                lut[i + 1] = y ? 255 : 0;
                lut[i + 2] = z ? 255 : 0;
                lut[i + 3] = 255;
            }
        }
    }
    return lut;
}

} // namespace

TEST(AdvancedProcessing, ShaderCacheCompilesAdvancedShaders) {
    Fixture fx;

    D3D11ProcessingShaderCache cache;
    cache.Initialize(fx.processing);
    CHECK(!cache.GetComputeShader("AdvancedTransformRgba.hlsl").Empty());
    CHECK(!cache.GetComputeShader("ApplyLut3D.hlsl").Empty());
}

TEST(AdvancedProcessing, PublicTypesHaveExpectedDefaults) {
    AffineTransformDesc affine = {};
    CHECK(affine.dstToSrc[0] == 1.0f);
    CHECK(affine.dstToSrc[4] == 1.0f);
    CHECK(affine.filter == ProcessingFilter::Linear);

    PerspectiveTransformDesc perspective = {};
    CHECK(perspective.dstToSrc[0] == 1.0f);
    CHECK(perspective.dstToSrc[4] == 1.0f);
    CHECK(perspective.dstToSrc[8] == 1.0f);

    Lut3DDesc lut = {};
    CHECK(lut.strength == 1.0f);
    CHECK(lut.preserveAlpha);
}

TEST(AdvancedProcessing, ProcessorInitializesAndCreatesOutputTexture) {
    Fixture fx;
    if (!fx.processing.SupportsRgba8Uav()) {
        return;
    }

    D3D11AdvancedProcessor processor;
    processor.Initialize(fx.processing);

    auto output = processor.CreateOutputTexture(*fx.core, 16, 8, DXGI_FORMAT_R8G8B8A8_UNORM);
    CHECK(output.Get() != nullptr);
    const auto desc = output.GetTexture2DDesc();
    CHECK_EQ(desc.Width, 16u);
    CHECK_EQ(desc.Height, 8u);
    CHECK((desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) != 0);
}

TEST(AdvancedProcessing, AffineIdentityMatchesSource) {
    Fixture fx;
    if (!fx.processing.SupportsRgba8Uav()) {
        return;
    }

    const std::vector<uint8_t> srcBytes = {
        255, 0,   0,   255,   0,   255, 0,   255,
        0,   0,   255, 255,   255, 255, 0,   255,
    };
    auto src = CreateTexture2DFromMemory(
        *fx.core, srcBytes.data(), 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM, 2 * 4,
        D3D11_BIND_SHADER_RESOURCE);

    D3D11AdvancedProcessor processor;
    processor.Initialize(fx.processing);
    auto dst = processor.CreateOutputTexture(*fx.core, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM);

    AffineTransformDesc desc = {};
    desc.filter = ProcessingFilter::Point;
    desc.dstRect = ProcessingRect{ 0, 0, 2, 2 };

    processor.DispatchAffineTransform(fx.core->GetImmediateContext(), src, dst, desc);
    const auto actual = ReadbackRgba8(*fx.core, dst);
    RequireBytesEqual(actual, srcBytes, "AffineIdentityMatchesSource");
}

TEST(AdvancedProcessing, PerspectiveIdentityMatchesSource) {
    Fixture fx;
    if (!fx.processing.SupportsRgba8Uav()) {
        return;
    }

    const std::vector<uint8_t> srcBytes = {
        10,  20,  30,  255,   40,  50,  60,  255,
        70,  80,  90,  255,   100, 110, 120, 255,
    };
    auto src = CreateTexture2DFromMemory(
        *fx.core, srcBytes.data(), 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM, 2 * 4,
        D3D11_BIND_SHADER_RESOURCE);

    D3D11AdvancedProcessor processor;
    processor.Initialize(fx.processing);
    auto dst = processor.CreateOutputTexture(*fx.core, 2, 2, DXGI_FORMAT_R8G8B8A8_UNORM);

    PerspectiveTransformDesc desc = {};
    desc.filter = ProcessingFilter::Point;
    desc.dstRect = ProcessingRect{ 0, 0, 2, 2 };

    processor.DispatchPerspectiveTransform(fx.core->GetImmediateContext(), src, dst, desc);
    const auto actual = ReadbackRgba8(*fx.core, dst);
    RequireBytesEqual(actual, srcBytes, "PerspectiveIdentityMatchesSource");
}

TEST(AdvancedProcessing, Lut3DIdentityPreservesColorAndAlpha) {
    Fixture fx;
    if (!fx.processing.SupportsRgba8Uav()) {
        return;
    }

    const std::vector<uint8_t> srcBytes = { 255, 0, 0, 128 };
    auto src = CreateTexture2DFromMemory(
        *fx.core, srcBytes.data(), 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, 4,
        D3D11_BIND_SHADER_RESOURCE);
    auto lut = CreateTexture3DFromRgba8(*fx.core, 2, 2, 2, MakeIdentity2x2x2LutRgba8());

    D3D11AdvancedProcessor processor;
    processor.Initialize(fx.processing);
    auto dst = processor.CreateOutputTexture(*fx.core, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);

    Lut3DDesc desc = {};
    desc.dstRect = ProcessingRect{ 0, 0, 1, 1 };
    desc.strength = 1.0f;
    desc.preserveAlpha = true;

    processor.DispatchApplyLut3D(fx.core->GetImmediateContext(), src, lut, dst, desc);
    const auto actual = ReadbackRgba8(*fx.core, dst);
    RequireBytesEqual(actual, srcBytes, "Lut3DIdentityPreservesColorAndAlpha");
}
