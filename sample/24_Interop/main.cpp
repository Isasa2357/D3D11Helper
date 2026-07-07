//
// 24_Interop - shared texture, keyed mutex, and D3D11.4 fence helpers.
//
#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Interop/D3D11Interop.hpp>

#include <exception>
#include <iostream>

using namespace D3D11CoreLib;

int main() {
    try {
        D3D11CoreConfig config{};
        config.enableDebugLayer = true;
        config.enableInfoQueue = true;
        config.allowWarpAdapter = true;

        D3D11Core core;
        core.Initialize(config);

        ID3D11Device* device = core.GetDevice();
        ID3D11DeviceContext* context = core.GetImmediateContext();

        D3D11SharedTexture2DDesc sharedDesc{};
        sharedDesc.width = 64;
        sharedDesc.height = 64;
        sharedDesc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sharedDesc.syncMode = D3D11SharedTextureSyncMode::KeyedMutex;

        auto shared = CreateSharedTexture2DWithHandle(device, sharedDesc);
        auto opened = OpenSharedTexture2D(device, shared.handle);

        D3D11KeyedMutex producerMutex(shared.Get());
        D3D11KeyedMutex consumerMutex(opened.Get());

        producerMutex.AcquireOrThrow(0, 1000);
        producerMutex.Release(1);
        consumerMutex.AcquireOrThrow(1, 1000);
        consumerMutex.Release(0);

        std::cout << "Shared keyed-mutex texture created/opened and key handoff completed.\n";

        const auto fenceSupport = CheckD3D11FenceSupport(device, context);
        std::cout << "D3D11 fence support: "
                  << (fenceSupport.supported ? "yes" : "no")
                  << " (" << fenceSupport.reason << ")\n";

        if (fenceSupport.supported) {
            D3D11Fence fence;
            fence.Initialize(device);
            auto fenceHandle = fence.CreateSharedHandleOwned();

            D3D11Fence openedFence;
            openedFence.OpenSharedHandle(device, fenceHandle);

            fence.Signal(context, 1);
            core.Flush();
            openedFence.CpuWait(1);

            std::cout << "D3D11 fence signal/shared-open/CpuWait completed.\n";
        } else {
            std::cout << "Fence part skipped on this device/context.\n";
        }

        std::cout << "RESULT: OK\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Interop sample failed: " << e.what() << "\n";
        return 1;
    }
}
