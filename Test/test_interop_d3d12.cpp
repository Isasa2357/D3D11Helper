//
// test_interop_d3d12.cpp - optional D3D11/D3D12 end-to-end interop tests.
//
#include "TestUtil.hpp"

#include <D3D11Helper/D3D11Interop/D3D11Interop.hpp>
#include <D3D12Helper/D3D12Core/D3D12Core.hpp>
#include <D3D12Helper/D3D12Gpu/D3D12Gpu.hpp>
#include <D3D12Helper/D3D12Interop/D3D12Interop.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::shared_ptr<D3D12CoreLib::D3D12Core> MakeD3D12CoreForD3D11Adapter(D3D11CoreLib::D3D11Core& d3d11) {
    D3D12CoreLib::D3D12CoreConfig cfg;
    cfg.enableDebugLayer = true;
    cfg.enableInfoQueue = true;
    cfg.allowWarpAdapter = true;
    cfg.createCopyQueue = true;
    return D3D12CoreLib::D3D12Core::CreateSharedWithAdapterLuid(d3d11.GetAdapterLuid(), cfg);
}

std::vector<uint8_t> MakeRgbaPattern(UINT width, UINT height) {
    std::vector<uint8_t> data(static_cast<size_t>(width) * height * 4u);
    for (UINT y = 0; y < height; ++y) {
        for (UINT x = 0; x < width; ++x) {
            const size_t i = (static_cast<size_t>(y) * width + x) * 4u;
            data[i + 0] = static_cast<uint8_t>(10 + x + y * 3u);
            data[i + 1] = static_cast<uint8_t>(40 + x * 2u + y);
            data[i + 2] = static_cast<uint8_t>(80 + x + y * 2u);
            data[i + 3] = 255;
        }
    }
    return data;
}

void CheckPixels(const D3D12CoreLib::D3D12CpuImage& image, const std::vector<uint8_t>& expected) {
    if (image.width == 0 || image.height == 0 || image.planes.empty()) {
        throw std::runtime_error("invalid readback image");
    }
    const auto& p = image.planes[0];
    const UINT rowBytes = image.width * 4u;
    if (expected.size() != static_cast<size_t>(rowBytes) * image.height) {
        throw std::runtime_error("expected size mismatch");
    }
    for (UINT y = 0; y < image.height; ++y) {
        const auto* got = image.pixels.data() + p.offsetBytes + static_cast<size_t>(y) * p.rowPitch;
        const auto* exp = expected.data() + static_cast<size_t>(y) * rowBytes;
        for (UINT i = 0; i < rowBytes; ++i) {
            if (got[i] != exp[i]) {
                throw std::runtime_error("pixel mismatch");
            }
        }
    }
}

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

} // namespace

int main() {
    auto d3d11 = TestUtil::MakeCore();
    std::shared_ptr<D3D12CoreLib::D3D12Core> d3d12;

    try {
        d3d12 = MakeD3D12CoreForD3D11Adapter(*d3d11);
    } catch (const std::exception& e) {
        TestUtil::Log(std::string("Skipping D3D11/D3D12 interop tests: ") + e.what());
        return TestUtil::Result("InteropD3D12");
    }

    TEST_RUN("D3D11 shared texture opened by D3D12", {
        constexpr UINT width = 4;
        constexpr UINT height = 4;
        auto expected = MakeRgbaPattern(width, height);

        D3D11CoreLib::D3D11SharedTexture2DDesc desc;
        desc.width = width;
        desc.height = height;
        desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.bindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.syncMode = D3D11CoreLib::D3D11SharedTextureSyncMode::SharedFence;

        auto shared = D3D11CoreLib::CreateSharedTexture2DWithHandle(d3d11->GetDevice(), desc);
        if (!shared) {
            throw std::runtime_error("failed to create shared D3D11 texture");
        }

        d3d11->GetImmediateContext()->UpdateSubresource(
            shared.Get(), 0, nullptr, expected.data(), width * 4u, 0);
        d3d11->Flush();

        auto opened = D3D12CoreLib::OpenSharedResource(
            d3d12->GetDevice(), shared.GetHandle(), D3D12_RESOURCE_STATE_COMMON);
        if (!opened.Get()) {
            throw std::runtime_error("D3D12 did not open D3D11 shared texture");
        }
        if (!d3d12->IsSameAdapter(d3d11->GetAdapterLuid())) {
            throw std::runtime_error("D3D12 core is not on the D3D11 adapter");
        }

        auto image = D3D12CoreLib::ReadbackTexture2DToCpuImage(*d3d12, opened);
        CheckPixels(image, expected);
    });

    TEST_RUN("D3D12 shared fence opened by D3D11", {
        const auto support = D3D11CoreLib::CheckD3D11FenceSupport(
            d3d11->GetDevice(), d3d11->GetImmediateContext());
        if (!support.supported) {
            TestUtil::Log(std::string("Skipping shared fence E2E: ") + support.reason);
            return;
        }

        D3D12CoreLib::D3D12Fence fence12;
        fence12.InitializeShared(d3d12->GetDevice());
        auto fenceHandle = D3D12CoreLib::CreateSharedHandleForFence(d3d12->GetDevice(), fence12);
        if (!fenceHandle) {
            throw std::runtime_error("invalid shared fence handle");
        }

        D3D11CoreLib::D3D11Fence fence11;
        fence11.OpenSharedHandle(d3d11->GetDevice(), fenceHandle.Get());
        if (!fence11.IsInitialized()) {
            throw std::runtime_error("D3D11 did not open D3D12 fence");
        }

        fence12.Signal(d3d12->DirectQueue().Get(), 5);
        fence11.CpuWait(5);
        if (fence11.GetCompletedValue() < 5) {
            throw std::runtime_error("D3D11 fence did not observe D3D12 signal");
        }
    });

    TEST_RUN("Interop invalid arguments", {
        ExpectThrows("OpenSharedResource null device", [&] {
            (void)D3D12CoreLib::OpenSharedResource(nullptr, nullptr);
        });
        ExpectThrows("CreateSharedHandleForResource null device", [&] {
            D3D12CoreLib::D3D12Resource empty;
            (void)D3D12CoreLib::CreateSharedHandleForResource(nullptr, empty);
        });
    });

    return TestUtil::Result("InteropD3D12");
}
