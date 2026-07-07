//
// test_presentation_diagnostics_shader_edges.cpp
// D3D11-only edge tests for offscreen presentation, diagnostics, and shader compiler helpers.
//
#include "TestUtil.hpp"

#include <D3D11Helper/D3D11Presentation/D3D11Presentation.hpp>
#include <D3D11Helper/D3D11Diagnostics/D3D11Diagnostics.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace D3D11CoreLib;

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

void RequireNearByte(uint8_t actual, uint8_t expected, uint8_t tolerance, const char* label) {
    const int diff = (actual > expected) ? int(actual - expected) : int(expected - actual);
    if (diff > int(tolerance)) {
        throw std::runtime_error(
            std::string(label) + ": actual=" + std::to_string(actual) +
            " expected=" + std::to_string(expected));
    }
}

std::vector<uint8_t> PackedBytes(const D3D11CpuImage& image) {
    ValidateCpuImage(image, "PackedBytes");
    const auto& plane = image.planes[0];
    const UINT rowBytes = GetPackedRowPitch(image.width, image.format);
    return PackRows(image.pixels.data() + static_cast<size_t>(plane.offsetBytes),
                    plane.rowPitch,
                    rowBytes,
                    plane.height);
}

std::filesystem::path MakeTempDir(const char* name) {
    const auto dir = std::filesystem::temp_directory_path() /
        (std::string("D3D11Helper_") + name + "_" + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(dir);
    return dir;
}

void WriteTextFile(const std::filesystem::path& path, const std::string& text) {
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) {
        throw std::runtime_error("failed to open temp file for write: " + path.string());
    }
    ofs << text;
}

} // namespace

int main() {
    auto core = TestUtil::MakeCore();
    ID3D11Device* device = core->GetDevice();
    ID3D11DeviceContext* context = core->GetImmediateContext();

    TEST_RUN("RenderTarget viewport clear readback and reset", {
        D3D11RenderTargetDesc desc{};
        desc.width = 4;
        desc.height = 3;
        desc.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.depthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        desc.createShaderResource = true;
        desc.clearColor[0] = 0.25f;
        desc.clearColor[1] = 0.50f;
        desc.clearColor[2] = 0.75f;
        desc.clearColor[3] = 1.00f;
        desc.clearDepth = 0.5f;
        desc.clearStencil = 7;

        auto target = CreateRenderTarget(*core, desc);
        if (!target || !target.Rtv() || !target.Srv() || !target.Dsv()) {
            throw std::runtime_error("render target did not create expected views");
        }
        if (target.Width() != 4 || target.Height() != 3 || target.ColorFormat() != DXGI_FORMAT_R8G8B8A8_UNORM) {
            throw std::runtime_error("render target metadata mismatch");
        }

        const auto vp = target.Viewport(1.0f, 2.0f, 0.25f, 0.75f);
        if (vp.TopLeftX != 1.0f || vp.TopLeftY != 2.0f || vp.Width != 4.0f || vp.Height != 3.0f ||
            vp.MinDepth != 0.25f || vp.MaxDepth != 0.75f) {
            throw std::runtime_error("render target viewport mismatch");
        }

        target.BindAndSetViewport(context);
        UINT viewportCount = 1;
        D3D11_VIEWPORT current{};
        context->RSGetViewports(&viewportCount, &current);
        if (viewportCount != 1 || current.Width != 4.0f || current.Height != 3.0f) {
            throw std::runtime_error("RSGetViewports did not observe render target viewport");
        }

        target.Clear(context);
        core->Flush();
        auto image = ReadbackTexture2DToCpuImage(*core, target.ColorTexture());
        const auto bytes = PackedBytes(image);
        for (size_t i = 0; i < bytes.size(); i += 4) {
            RequireNearByte(bytes[i + 0], 64, 2, "clear r");
            RequireNearByte(bytes[i + 1], 128, 2, "clear g");
            RequireNearByte(bytes[i + 2], 191, 3, "clear b");
            RequireNearByte(bytes[i + 3], 255, 1, "clear a");
        }

        UnbindRenderTargets(context, 1);
        UnbindRenderTargets(context, 0);
        target.Reset();
        if (target || target.Rtv() || target.Srv() || target.Dsv()) {
            throw std::runtime_error("render target reset did not clear views");
        }
        ExpectThrows("Bind after reset", [&] {
            target.Bind(context);
        });
    });

    TEST_RUN("RenderTarget validation edge cases", {
        D3D11RenderTargetDesc desc{};
        desc.width = 1;
        desc.height = 1;

        ExpectThrows("zero render target width", [&] {
            auto bad = desc;
            bad.width = 0;
            (void)CreateRenderTarget(*core, bad);
        });
        ExpectThrows("unknown color format", [&] {
            auto bad = desc;
            bad.colorFormat = DXGI_FORMAT_UNKNOWN;
            (void)CreateRenderTarget(*core, bad);
        });
        ExpectThrows("depth as color format", [&] {
            auto bad = desc;
            bad.colorFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
            (void)CreateRenderTarget(*core, bad);
        });
        ExpectThrows("non-depth depth format", [&] {
            auto bad = desc;
            bad.depthFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
            (void)CreateRenderTarget(*core, bad);
        });
        ExpectThrows("MSAA render target unsupported", [&] {
            auto bad = desc;
            bad.sampleCount = 2;
            (void)CreateRenderTarget(*core, bad);
        });
        ExpectThrows("SetViewport null context", [&] {
            SetViewport(nullptr, 1, 1);
        });
        ExpectThrows("Unbind too many render targets", [&] {
            UnbindRenderTargets(context, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT + 1);
        });
    });

    TEST_RUN("Diagnostics device lost and InfoQueue edge cases", {
        if (DeviceLostReasonName(S_OK) != std::string("S_OK")) {
            throw std::runtime_error("unexpected S_OK device-lost name");
        }
        if (!IsDeviceLost(DXGI_ERROR_DEVICE_REMOVED) || !IsDeviceLost(DXGI_ERROR_DEVICE_HUNG) ||
            !IsDeviceLost(DXGI_ERROR_INVALID_CALL) || !IsDeviceLost(E_OUTOFMEMORY)) {
            throw std::runtime_error("known device-lost HRESULT was not classified");
        }
        if (IsDeviceLost(S_OK) || DeviceLostReasonName(static_cast<HRESULT>(0x12345678)) != std::string("UNKNOWN_HRESULT")) {
            throw std::runtime_error("unexpected device-lost classification for non-lost HRESULT");
        }

        const auto lost = CheckDeviceLost(device);
        if (lost.name == nullptr || lost.name[0] == '\0') {
            throw std::runtime_error("CheckDeviceLost returned empty name");
        }
        ThrowIfDeviceLost(device);
        ExpectThrows("CheckDeviceLost null", [&] {
            (void)CheckDeviceLost(nullptr);
        });

        ExpectThrows("InfoQueue null device", [&] {
            (void)TryGetD3D11InfoQueue(nullptr);
        });
        ExpectThrows("InfoQueue count null", [&] {
            (void)GetInfoQueueMessageCount(nullptr);
        });
        ExpectThrows("InfoQueue clear null", [&] {
            ClearInfoQueueMessages(nullptr);
        });
        ExpectThrows("InfoQueue break null", [&] {
            SetInfoQueueBreakOnSeverity(nullptr, D3D11_MESSAGE_SEVERITY_WARNING, false);
        });
        ExpectThrows("InfoQueue filter null queue", [&] {
            D3D11_MESSAGE_SEVERITY severity = D3D11_MESSAGE_SEVERITY_INFO;
            AddInfoQueueStorageFilterDenySeverity(nullptr, &severity, 1);
        });

        auto queue = TryGetD3D11InfoQueue(device);
        if (queue) {
            const auto before = GetInfoQueueMessageCount(queue.Get());
            (void)before;
            ClearInfoQueueMessages(queue.Get());
            const auto afterClear = GetInfoQueueMessageCount(queue.Get());
            (void)afterClear;
            SetInfoQueueBreakOnSeverity(queue.Get(), D3D11_MESSAGE_SEVERITY_WARNING, false);
            AddInfoQueueStorageFilterDenySeverity(queue.Get(), nullptr, 0);
            D3D11_MESSAGE_SEVERITY deny = D3D11_MESSAGE_SEVERITY_INFO;
            AddInfoQueueStorageFilterDenySeverity(queue.Get(), &deny, 1);
            ExpectThrows("InfoQueue nonzero count null severities", [&] {
                AddInfoQueueStorageFilterDenySeverity(queue.Get(), nullptr, 1);
            });
        } else {
            TestUtil::Log("InfoQueue unavailable; optional InfoQueue runtime checks skipped");
        }
    });

    TEST_RUN("ShaderCompiler include define file load and failure paths", {
        const std::string source =
            "#ifndef FACTOR\n"
            "#define FACTOR 1\n"
            "#endif\n"
            "float4 main(float4 p : SV_Position) : SV_Target { return float4(FACTOR, 0, 0, 1); }\n";

        ShaderCompileDesc desc{};
        desc.entryPoint = "main";
        desc.target = "ps_5_0";
        desc.defines.push_back({ "FACTOR", "1" });
        auto direct = CompileShaderFromSource(source, desc, "macro_test.hlsl");
        if (direct.Empty()) {
            throw std::runtime_error("CompileShaderFromSource returned empty bytecode");
        }

        const auto tempDir = MakeTempDir("shader_edges");
        const auto includePath = tempDir / "Common.hlsli";
        const auto sourcePath = tempDir / "Main.hlsl";
        const auto csoPath = tempDir / "shader.cso";

        WriteTextFile(includePath, "float4 MakeColor() { return float4(0, 1, 0, 1); }\n");
        WriteTextFile(sourcePath,
                      "#include \"Common.hlsli\"\n"
                      "float4 main(float4 p : SV_Position) : SV_Target { return MakeColor(); }\n");

        ShaderCompileDesc fileDesc{};
        fileDesc.sourcePath = sourcePath;
        fileDesc.entryPoint = "main";
        fileDesc.target = "ps_5_0";
        auto fromFile = CompileShaderFromFile(fileDesc);
        if (fromFile.Empty()) {
            throw std::runtime_error("CompileShaderFromFile returned empty bytecode");
        }

        {
            std::ofstream cso(csoPath, std::ios::binary);
            if (!cso) {
                throw std::runtime_error("failed to create temp cso file");
            }
            const auto* data = static_cast<const uint8_t*>(fromFile.Data());
            cso.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(fromFile.Size()));
        }
        auto loaded = LoadShaderBytecodeFromFile(csoPath);
        if (loaded.Empty() || loaded.Size() != fromFile.Size()) {
            throw std::runtime_error("LoadShaderBytecodeFromFile did not load expected bytecode size");
        }

        ExpectThrows("empty shader source", [&] {
            (void)CompileShaderFromSource_D3DCompile("", "main", "ps_5_0");
        });
        ExpectThrows("bad shader source", [&] {
            (void)CompileShaderFromSource_D3DCompile("this is not hlsl", "main", "ps_5_0");
        });
        ExpectThrows("missing shader file", [&] {
            ShaderCompileDesc missing{};
            missing.sourcePath = tempDir / "missing.hlsl";
            missing.entryPoint = "main";
            missing.target = "ps_5_0";
            (void)CompileShaderFromFile(missing);
        });
        ExpectThrows("empty cso file", [&] {
            const auto emptyPath = tempDir / "empty.cso";
            WriteTextFile(emptyPath, "");
            (void)LoadShaderBytecodeFromFile(emptyPath);
        });
        ExpectThrows("empty define name", [&] {
            ShaderCompileDesc badDesc{};
            badDesc.entryPoint = "main";
            badDesc.target = "ps_5_0";
            badDesc.defines.push_back({ "", "1" });
            (void)CompileShaderFromSource(source, badDesc, "bad_define.hlsl");
        });

        std::error_code ec;
        std::filesystem::remove_all(tempDir, ec);
    });

    return TestUtil::Result("PresentationDiagnosticsShaderEdges");
}
