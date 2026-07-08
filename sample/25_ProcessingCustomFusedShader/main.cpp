#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>
#include <D3D11Helper/D3D11Processing/D3D11Processing.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <vector>
#include <utility>

using namespace D3D11CoreLib;
using namespace D3D11CoreLib::Processing;

namespace {

constexpr UINT kThreadGroupX = 16;
constexpr UINT kThreadGroupY = 16;

struct ProcessingConstantsForSample {
    UINT srcWidth = 0;
    UINT srcHeight = 0;
    UINT dstWidth = 0;
    UINT dstHeight = 0;

    INT srcX = 0;
    INT srcY = 0;
    INT dstX = 0;
    INT dstY = 0;

    UINT srcFormat = 0;
    UINT dstFormat = 0;
    UINT srcMatrix = 0;
    UINT srcRange = 0;

    UINT dstMatrix = 0;
    UINT dstRange = 0;
    UINT filter = 0;
    UINT alphaMode = 0;

    float scaleX = 1.0f;
    float scaleY = 1.0f;
    UINT mapFormat = 0;
    UINT coordinateMode = 0;

    UINT borderMode = 0;
    UINT blendMode = 0;
    float opacity = 1.0f;
    UINT reserved0 = 0;

    float borderColor[4] = { 0, 0, 0, 0 };

    UINT baseWidth = 0;
    UINT baseHeight = 0;
    UINT overlayWidth = 0;
    UINT overlayHeight = 0;

    INT baseX = 0;
    INT baseY = 0;
    INT overlayX = 0;
    INT overlayY = 0;
};
static_assert((sizeof(ProcessingConstantsForSample) % 16) == 0, "constant buffer size must be 16-byte aligned");

UINT DivideRoundUp(UINT value, UINT divisor) noexcept {
    return (value + divisor - 1u) / divisor;
}

std::filesystem::path ProcessingShaderDir() {
    const auto namespacedRuntimeDir =
        std::filesystem::current_path() / "D3D11Helper" / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(namespacedRuntimeDir / "ProcessingCommon.hlsli")) {
        return namespacedRuntimeDir;
    }

    const auto legacyRuntimeDir = std::filesystem::current_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(legacyRuntimeDir / "ProcessingCommon.hlsli")) {
        return legacyRuntimeDir;
    }

#ifdef D3D11HELPER_SAMPLE_SOURCE_DIR
    const auto sourceDir = std::filesystem::u8path(D3D11HELPER_SAMPLE_SOURCE_DIR)
        .parent_path() / "shaders" / "D3D11Processing";
    if (std::filesystem::exists(sourceDir / "ProcessingCommon.hlsli")) {
        return sourceDir;
    }
#endif

    return namespacedRuntimeDir;
}

std::filesystem::path SampleShaderPath() {
#ifdef D3D11HELPER_SAMPLE_SOURCE_DIR
    const auto sourcePath = std::filesystem::u8path(D3D11HELPER_SAMPLE_SOURCE_DIR) /
        "25_ProcessingCustomFusedShader" / "CustomNv12ResizeDarken.hlsl";
    if (std::filesystem::exists(sourcePath)) {
        return sourcePath;
    }
#endif

    const auto runtimePath = std::filesystem::current_path() / "CustomNv12ResizeDarken.hlsl";
    return runtimePath;
}

D3D11Resource CreateNv12Gradient(ID3D11Device* device, UINT width, UINT height) {
    if ((width % 2u) != 0 || (height % 2u) != 0) {
        throw std::runtime_error("NV12 sample texture dimensions must be even");
    }

    std::vector<uint8_t> data(static_cast<size_t>(width) * height + static_cast<size_t>(width) * (height / 2u));

    for (UINT y = 0; y < height; ++y) {
        for (UINT x = 0; x < width; ++x) {
            data[static_cast<size_t>(y) * width + x] = static_cast<uint8_t>((x * 255u) / std::max(1u, width - 1u));
        }
    }

    uint8_t* uv = data.data() + static_cast<size_t>(width) * height;
    for (UINT y = 0; y < height / 2u; ++y) {
        for (UINT x = 0; x < width; x += 2u) {
            uv[static_cast<size_t>(y) * width + x + 0u] = 128;
            uv[static_cast<size_t>(y) * width + x + 1u] = 128;
        }
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_NV12;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = data.data();
    init.SysMemPitch = width;

    ComPtr<ID3D11Texture2D> texture;
    D3D11CORE_THROW_IF_FAILED(device->CreateTexture2D(&desc, &init, &texture));
    return D3D11Resource(std::move(texture));
}

void DispatchCustomFusedShader(
    D3D11Core& core,
    D3D11ProcessingContext& processing,
    D3D11Resource& srcNv12,
    D3D11Resource& dstRgba) {

    ShaderCompileDesc compileDesc = {};
    compileDesc.sourcePath = SampleShaderPath();
    compileDesc.entryPoint = "main";
    compileDesc.target = "cs_5_0";
    compileDesc.includeDirs.push_back(processing.ShaderDirectory());
    compileDesc.includeDirs.push_back(compileDesc.sourcePath.parent_path());

    auto bytecode = CompileShaderFromFile(compileDesc);
    D3D11ComputePipeline pipeline;
    pipeline.Initialize(core.GetDevice(), bytecode);

    auto srcViews = CreateYuv420SrvViewSet(processing, srcNv12);
    auto dstViews = CreateRgbaTextureViewSet(processing, dstRgba, false, true, DXGI_FORMAT_R8G8B8A8_UNORM);

    ProcessingConstantsForSample constants = {};
    constants.srcWidth = static_cast<UINT>(srcNv12.GetWidth());
    constants.srcHeight = srcNv12.GetHeight();
    constants.dstWidth = static_cast<UINT>(dstRgba.GetWidth());
    constants.dstHeight = dstRgba.GetHeight();
    constants.srcFormat = static_cast<UINT>(DXGI_FORMAT_NV12);
    constants.dstFormat = static_cast<UINT>(DXGI_FORMAT_R8G8B8A8_UNORM);
    constants.srcMatrix = static_cast<UINT>(ProcessingColorMatrix::BT709);
    constants.srcRange = static_cast<UINT>(ProcessingColorRange::Full);
    constants.dstMatrix = static_cast<UINT>(ProcessingColorMatrix::BT709);
    constants.dstRange = static_cast<UINT>(ProcessingColorRange::Full);
    constants.filter = static_cast<UINT>(ProcessingFilter::Linear);
    constants.alphaMode = static_cast<UINT>(ProcessingAlphaMode::Ignore);
    constants.scaleX = static_cast<float>(constants.srcWidth) / static_cast<float>(constants.dstWidth);
    constants.scaleY = static_cast<float>(constants.srcHeight) / static_cast<float>(constants.dstHeight);

    auto* context = core.GetImmediateContext();
    processing.UpdateConstants(context, &constants, static_cast<UINT>(sizeof(constants)));

    ID3D11Buffer* cb = processing.ConstantBuffer();
    ID3D11ShaderResourceView* srvs[] = { srcViews.ySrv.Get(), srcViews.uvSrv.Get() };
    ID3D11UnorderedAccessView* uavs[] = { dstViews.uav.Get() };
    UINT initialCounts[] = { static_cast<UINT>(-1) };

    context->CSSetConstantBuffers(0, 1, &cb);
    context->CSSetShaderResources(0, 2, srvs);
    context->CSSetUnorderedAccessViews(0, 1, uavs, initialCounts);

    pipeline.Dispatch(
        context,
        DivideRoundUp(constants.dstWidth, kThreadGroupX),
        DivideRoundUp(constants.dstHeight, kThreadGroupY),
        1);

    ID3D11ShaderResourceView* nullSrvs[2] = {};
    ID3D11UnorderedAccessView* nullUavs[1] = {};
    ID3D11Buffer* nullCb = nullptr;
    context->CSSetShaderResources(0, 2, nullSrvs);
    context->CSSetUnorderedAccessViews(0, 1, nullUavs, nullptr);
    context->CSSetConstantBuffers(0, 1, &nullCb);
}

} // namespace

int main() {
    try {
        D3D11CoreConfig cfg;
        cfg.allowWarpAdapter = true;
        auto core = D3D11Core::CreateShared(cfg);

        D3D11ProcessingContext processing;
        processing.Initialize(*core, ProcessingShaderDir());

        if (!processing.SupportsNv12Srv() || !processing.SupportsRgba8Uav()) {
            std::cout << "Required format support unavailable.\n";
            return 0;
        }

        constexpr UINT srcWidth = 128;
        constexpr UINT srcHeight = 72;
        constexpr UINT dstWidth = 64;
        constexpr UINT dstHeight = 36;

        auto src = CreateNv12Gradient(core->GetDevice(), srcWidth, srcHeight);
        auto dst = CreateTexture2D(
            *core,
            dstWidth,
            dstHeight,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);

        DispatchCustomFusedShader(*core, processing, src, dst);
        core->Flush();

        auto readback = ReadbackTexture2DToCpuImage(*core, dst);
        const auto& plane = readback.planes[0];
        const UINT cx = dstWidth / 2u;
        const UINT cy = dstHeight / 2u;
        const auto* center = readback.pixels.data()
            + plane.offsetBytes
            + static_cast<size_t>(cy) * plane.rowPitch
            + static_cast<size_t>(cx) * 4u;

        std::cout << "RESULT: OK - custom HLSL fused NV12 -> RGB -> resize -> outside darken dispatch completed.\n";
        std::cout << "Center pixel RGBA = ("
                  << static_cast<int>(center[0]) << ", "
                  << static_cast<int>(center[1]) << ", "
                  << static_cast<int>(center[2]) << ", "
                  << static_cast<int>(center[3]) << ")\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
