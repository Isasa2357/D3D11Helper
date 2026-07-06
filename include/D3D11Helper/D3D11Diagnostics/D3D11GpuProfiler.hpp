#pragma once
//
// D3D11GpuProfiler.hpp - simple single-frame GPU profiler
//
#include <D3D11Helper/D3D11Diagnostics/D3D11GpuTimer.hpp>

#include <string>
#include <vector>

namespace D3D11CoreLib {

struct D3D11GpuProfilerSampleResult {
    std::string name;
    bool completed = false;
    bool valid = false;
    bool disjoint = false;
    double milliseconds = 0.0;
};

class D3D11GpuProfiler {
public:
    void Initialize(ID3D11Device* device);
    void Reset();

    bool IsInitialized() const noexcept { return m_initialized; }

    void BeginFrame(ID3D11DeviceContext* context);
    void EndFrame(ID3D11DeviceContext* context);

    UINT BeginSample(ID3D11DeviceContext* context, std::string name);
    void EndSample(ID3D11DeviceContext* context, UINT sampleId);

    std::vector<D3D11GpuProfilerSampleResult> Resolve(
        ID3D11DeviceContext* context,
        bool wait = true);

private:
    struct Sample {
        std::string name;
        D3D11GpuTimer timer;
        bool ended = false;
    };

    ComPtr<ID3D11Device> m_device;
    std::vector<Sample> m_samples;
    bool m_initialized = false;
    bool m_frameActive = false;
    bool m_frameEnded = false;
};

class D3D11ScopedGpuProfileSample {
public:
    D3D11ScopedGpuProfileSample(
        D3D11GpuProfiler& profiler,
        ID3D11DeviceContext* context,
        std::string name);

    ~D3D11ScopedGpuProfileSample() noexcept;

    D3D11ScopedGpuProfileSample(const D3D11ScopedGpuProfileSample&) = delete;
    D3D11ScopedGpuProfileSample& operator=(const D3D11ScopedGpuProfileSample&) = delete;

private:
    D3D11GpuProfiler& m_profiler;
    ID3D11DeviceContext* m_context = nullptr;
    UINT m_sampleId = 0;
    bool m_active = false;
};

} // namespace D3D11CoreLib
