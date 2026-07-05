#include "TestUtil.hpp"

#include <atomic>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace D3D11CoreLib;

int main()
{
    auto core = TestUtil::MakeCore();

    TestUtil::Run("CreateDeferredContext", [&] {
        D3D11DeferredContext dc = core->CreateDeferredContext();
        if (!dc || !dc.Get()) {
            throw std::runtime_error("failed to create deferred context");
        }
    });

    TestUtil::Run("FinishAndExecuteEmptyCommandList", [&] {
        D3D11DeferredContext dc = core->CreateDeferredContext();
        if (!dc || !dc.Get()) {
            throw std::runtime_error("failed to create deferred context");
        }

        dc->ClearState();

        ComPtr<ID3D11CommandList> list = dc.FinishCommandList(false);
        if (!list) {
            throw std::runtime_error("FinishCommandList returned null");
        }

        core->ExecuteCommandList(list.Get());
        core->Flush();
    });

    TestUtil::Run("ParallelRecordFinishAndExecute", [&] {
        constexpr int kThreadCount = 4;

        std::vector<ComPtr<ID3D11CommandList>> lists(kThreadCount);
        std::vector<std::thread> threads;
        std::atomic<int> failures{0};

        for (int i = 0; i < kThreadCount; ++i) {
            threads.emplace_back([&, i] {
                try {
                    D3D11DeferredContext dc = core->CreateDeferredContext();
                    if (!dc || !dc.Get()) {
                        throw std::runtime_error("failed to create deferred context");
                    }

                    dc->ClearState();

                    lists[i] = dc.FinishCommandList(false);
                    if (!lists[i]) {
                        throw std::runtime_error("FinishCommandList returned null");
                    }
                } catch (...) {
                    ++failures;
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        if (failures.load() != 0) {
            throw std::runtime_error("worker thread failed");
        }

        for (auto& list : lists) {
            if (!list) {
                throw std::runtime_error("null command list");
            }
            core->ExecuteCommandList(list.Get());
        }

        core->Flush();
    });

    return TestUtil::Result("test_deferred_context");
}
