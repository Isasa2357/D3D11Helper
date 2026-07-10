//
// test_interop_fence.cpp - D3D11 fence support and fence-point tests
//
#include "TestUtil.hpp"

#include <D3D11Helper/D3D11Interop/D3D11Interop.hpp>

#include <functional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

using namespace D3D11CoreLib;

namespace {
void ExpectThrows(const char* label, const std::function<void()>& fn) {
    bool threw = false;
    try { fn(); } catch (...) { threw = true; }
    if (!threw) throw std::runtime_error(std::string(label) + " did not throw");
}
} // namespace

int main() {
    static_assert(std::is_copy_constructible_v<D3D11FencePoint>);
    static_assert(std::is_move_constructible_v<D3D11FencePoint>);

    auto core = TestUtil::MakeCore();
    ID3D11Device* device = core->GetDevice();
    ID3D11DeviceContext* context = core->GetImmediateContext();

    TEST_RUN("Fence support query", {
        const auto info = CheckD3D11FenceSupport(device, context);
        if (info.reason == nullptr || info.reason[0] == '\0')
            throw std::runtime_error("support reason is empty");
        if (info.supported != (info.hasDevice5 && info.hasContext4))
            throw std::runtime_error("support flag mismatch");
        if (IsD3D11FenceSupported(device, context) != info.supported)
            throw std::runtime_error("IsD3D11FenceSupported mismatch");
    });

    TEST_RUN("Invalid fence point checks", {
        D3D11FencePoint point;
        if (point || point.IsComplete())
            throw std::runtime_error("default fence point is valid");
        ExpectThrows("invalid point CpuWait", [&] { point.CpuWait(); });
        D3D11Fence fence;
        ExpectThrows("invalid point GpuWait", [&] { fence.GpuWaitPoint(context, point); });
    });

    const auto support = CheckD3D11FenceSupport(device, context);
    if (!support.supported) {
        TestUtil::Log(std::string("Skipping D3D11Fence runtime tests: ") + support.reason);
        return TestUtil::Result("InteropFence");
    }

    TEST_RUN("Fence initialize signal cpu wait", {
        D3D11Fence fence;
        if (fence.IsInitialized()) throw std::runtime_error("fresh fence reports initialized");
        fence.Initialize(device);
        if (!fence.IsInitialized() || !fence.Get())
            throw std::runtime_error("fence did not initialize");
        fence.Signal(context, 1);
        core->Flush();
        fence.CpuWait(1);
        if (fence.GetCompletedValue() < 1)
            throw std::runtime_error("completed value did not reach 1");
    });

    TEST_RUN("Fence point signal and cpu wait", {
        D3D11Fence fence;
        fence.Initialize(device);
        auto point = fence.SignalPoint(context, 3);
        if (!point || point.GetFence() != fence.Get() || point.GetValue() != 3)
            throw std::runtime_error("fence point contents mismatch");
        core->Flush();
        point.CpuWait();
        if (!point.IsComplete()) throw std::runtime_error("point did not complete");
    });

    TEST_RUN("Fence point keeps fence alive", {
        D3D11FencePoint point;
        {
            D3D11Fence fence;
            fence.Initialize(device);
            point = fence.SignalPoint(context, 4);
        }
        core->Flush();
        point.CpuWait();
        if (!point.IsComplete())
            throw std::runtime_error("point lost fence lifetime");
    });

    TEST_RUN("Fence shared handle open", {
        D3D11Fence fence;
        fence.Initialize(device);
        auto handle = fence.CreateSharedHandleOwned();
        if (!handle) throw std::runtime_error("owned fence shared handle is invalid");
        D3D11Fence opened;
        opened.OpenSharedHandle(device, handle);
        if (!opened.IsInitialized()) throw std::runtime_error("opened fence did not initialize");
        fence.Signal(context, 2);
        core->Flush();
        opened.CpuWait(2);
        if (opened.GetCompletedValue() < 2)
            throw std::runtime_error("opened fence did not observe signal");
    });

    TEST_RUN("Fence move semantics", {
        static_assert(std::is_move_constructible_v<D3D11Fence>);
        static_assert(!std::is_copy_constructible_v<D3D11Fence>);
        D3D11Fence fence;
        fence.Initialize(device);
        D3D11Fence moved(std::move(fence));
        if (!moved.IsInitialized() || fence.IsInitialized())
            throw std::runtime_error("move construction failed");
        D3D11Fence assigned;
        assigned = std::move(moved);
        if (!assigned.IsInitialized() || moved.IsInitialized())
            throw std::runtime_error("move assignment failed");
    });

    TEST_RUN("Fence invalid argument checks", {
        D3D11Fence fence;
        ExpectThrows("Signal before initialize", [&] { fence.Signal(context, 1); });
        ExpectThrows("SignalPoint before initialize", [&] { (void)fence.SignalPoint(context, 1); });
        ExpectThrows("CpuWait before initialize", [&] { fence.CpuWait(1); });
        ExpectThrows("CreateSharedHandle before initialize", [&] { (void)fence.CreateSharedHandleOwned(); });
        ExpectThrows("Initialize null device", [&] { fence.Initialize(static_cast<ID3D11Device*>(nullptr)); });
        ExpectThrows("Open invalid raw handle", [&] { fence.OpenSharedHandle(device, static_cast<HANDLE>(nullptr)); });
        ExpectThrows("Open invalid owned handle", [&] {
            D3D11SharedHandle invalid;
            fence.OpenSharedHandle(device, invalid);
        });
    });

    return TestUtil::Result("InteropFence");
}
