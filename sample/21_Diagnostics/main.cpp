//
// 21_Diagnostics - minimal diagnostics console sample
//
#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Diagnostics/D3D11Diagnostics.hpp>

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

        const auto lostInfo = CheckDeviceLost(device);
        std::cout << "Device lost: " << (lostInfo.deviceLost ? "yes" : "no")
                  << " (" << lostInfo.name << ")\n";

        auto queue = TryGetD3D11InfoQueue(device);
        if (queue) {
            std::cout << "InfoQueue available, stored messages: "
                      << GetInfoQueueMessageCount(queue.Get()) << "\n";
        } else {
            std::cout << "InfoQueue unavailable\n";
        }

        D3D11GpuTimer timer;
        timer.Initialize(device);
        timer.Begin(context);
        timer.End(context);
        context->Flush();
        const auto timerResult = timer.Resolve(context, true);
        std::cout << "Timer completed: " << (timerResult.completed ? "yes" : "no")
                  << ", valid: " << (timerResult.valid ? "yes" : "no")
                  << ", ms: " << timerResult.milliseconds << "\n";

        D3D11GpuProfiler profiler;
        profiler.Initialize(device);
        profiler.BeginFrame(context);
        const UINT sampleA = profiler.BeginSample(context, "empty-a");
        profiler.EndSample(context, sampleA);
        const UINT sampleB = profiler.BeginSample(context, "empty-b");
        profiler.EndSample(context, sampleB);
        profiler.EndFrame(context);
        context->Flush();

        const auto samples = profiler.Resolve(context, true);
        for (const auto& sample : samples) {
            std::cout << "Sample " << sample.name
                      << ": completed=" << (sample.completed ? "yes" : "no")
                      << ", valid=" << (sample.valid ? "yes" : "no")
                      << ", ms=" << sample.milliseconds << "\n";
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Diagnostics sample failed: " << e.what() << "\n";
        return 1;
    }
}
