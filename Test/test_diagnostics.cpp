//
// test_diagnostics.cpp - v1.5.0 diagnostics runtime tests
//
#include "TestUtil.hpp"

#include <D3D11Helper/D3D11Diagnostics/D3D11Diagnostics.hpp>

#include <stdexcept>

using namespace D3D11CoreLib;

int main() {
    auto core = TestUtil::MakeCore();
    ID3D11Device* device = core->GetDevice();
    ID3D11DeviceContext* context = core->GetImmediateContext();

    TEST_RUN("DeviceLost classification", {
        if (IsDeviceLost(S_OK)) throw std::runtime_error("S_OK classified as device lost");
        if (!IsDeviceLost(DXGI_ERROR_DEVICE_HUNG)) throw std::runtime_error("DEVICE_HUNG not classified");
        if (!IsDeviceLost(DXGI_ERROR_DEVICE_REMOVED)) throw std::runtime_error("DEVICE_REMOVED not classified");
        if (!IsDeviceLost(DXGI_ERROR_DEVICE_RESET)) throw std::runtime_error("DEVICE_RESET not classified");
        if (!IsDeviceLost(DXGI_ERROR_DRIVER_INTERNAL_ERROR)) throw std::runtime_error("DRIVER_INTERNAL_ERROR not classified");
        if (!IsDeviceLost(DXGI_ERROR_INVALID_CALL)) throw std::runtime_error("INVALID_CALL not classified");
        if (!IsDeviceLost(E_OUTOFMEMORY)) throw std::runtime_error("E_OUTOFMEMORY not classified");
        if (!DeviceLostReasonName(S_OK)[0]) throw std::runtime_error("empty S_OK name");
        if (!DeviceLostReasonName(DXGI_ERROR_DEVICE_HUNG)[0]) throw std::runtime_error("empty hung name");
        if (!DeviceLostReasonName(DXGI_ERROR_DEVICE_REMOVED)[0]) throw std::runtime_error("empty removed name");
    });

    TEST_RUN("DeviceLost normal device check", {
        const auto info = CheckDeviceLost(device);
        if (!info.name || !info.name[0]) throw std::runtime_error("empty device lost name");
        if (info.deviceLost != IsDeviceLost(info.reason)) throw std::runtime_error("classification mismatch");
        ThrowIfDeviceLost(device);
    });

    TEST_RUN("InfoQueue helpers", {
        auto queue = TryGetD3D11InfoQueue(device);
        if (!queue) {
            TestUtil::Log("  InfoQueue unavailable; skipping InfoQueue operations");
            return;
        }
        (void)GetInfoQueueMessageCount(queue.Get());
        ClearInfoQueueMessages(queue.Get());
        SetInfoQueueBreakOnSeverity(queue.Get(), D3D11_MESSAGE_SEVERITY_ERROR, false);
        D3D11_MESSAGE_SEVERITY deny[] = { D3D11_MESSAGE_SEVERITY_INFO };
        AddInfoQueueStorageFilterDenySeverity(queue.Get(), deny, 1);
        AddInfoQueueStorageFilterDenySeverity(queue.Get(), nullptr, 0);
    });

    TEST_RUN("GpuTimer resolve", {
        D3D11GpuTimer timer;
        timer.Initialize(device);
        timer.Begin(context);
        timer.End(context);
        context->Flush();
        const auto result = timer.Resolve(context, true);
        if (!result.completed) throw std::runtime_error("timer did not complete");
        if (!result.disjoint && !result.valid) throw std::runtime_error("timer completed without valid result");
    });

    TEST_RUN("GpuProfiler samples", {
        D3D11GpuProfiler profiler;
        profiler.Initialize(device);
        profiler.BeginFrame(context);
        const UINT first = profiler.BeginSample(context, "first");
        profiler.EndSample(context, first);
        const UINT second = profiler.BeginSample(context, "second");
        profiler.EndSample(context, second);
        profiler.EndFrame(context);
        context->Flush();
        const auto results = profiler.Resolve(context, true);
        if (results.size() != 2) throw std::runtime_error("unexpected sample count");
        if (results[0].name != "first" || results[1].name != "second") throw std::runtime_error("unexpected sample names");
        profiler.Reset();
    });

    return TestUtil::Result("Diagnostics");
}
