//
// D3D11GpuProfiler.cpp
//
#include <D3D11Helper/D3D11Diagnostics/D3D11GpuProfiler.hpp>

#include <stdexcept>
#include <string>
#include <utility>

namespace D3D11CoreLib {
namespace {
void RequireProfilerContext(ID3D11DeviceContext* context, const char* functionName) {
    if (!context) {
        throw std::invalid_argument(std::string(functionName) + " requires a non-null context");
    }
}
}

void D3D11GpuProfiler::Initialize(ID3D11Device* device) {
    if (!device) {
        throw std::invalid_argument("D3D11GpuProfiler::Initialize requires a non-null device");
    }
    m_device = device;
    m_samples.clear();
    m_initialized = true;
    m_frameActive = false;
    m_frameEnded = false;
}

void D3D11GpuProfiler::Reset() {
    m_samples.clear();
    m_frameActive = false;
    m_frameEnded = false;
}

void D3D11GpuProfiler::BeginFrame(ID3D11DeviceContext* context) {
    RequireProfilerContext(context, "D3D11GpuProfiler::BeginFrame");
    if (!m_initialized) {
        throw std::runtime_error("D3D11GpuProfiler::BeginFrame requires an initialized profiler");
    }
    if (m_frameActive) {
        throw std::runtime_error("D3D11GpuProfiler::BeginFrame called while a frame is active");
    }
    m_samples.clear();
    m_frameActive = true;
    m_frameEnded = false;
}

void D3D11GpuProfiler::EndFrame(ID3D11DeviceContext* context) {
    RequireProfilerContext(context, "D3D11GpuProfiler::EndFrame");
    if (!m_frameActive) {
        throw std::runtime_error("D3D11GpuProfiler::EndFrame requires an active frame");
    }
    for (const auto& sample : m_samples) {
        if (!sample.ended) {
            throw std::runtime_error("D3D11GpuProfiler::EndFrame found an active sample");
        }
    }
    m_frameActive = false;
    m_frameEnded = true;
}

UINT D3D11GpuProfiler::BeginSample(ID3D11DeviceContext* context, std::string name) {
    RequireProfilerContext(context, "D3D11GpuProfiler::BeginSample");
    if (!m_initialized) {
        throw std::runtime_error("D3D11GpuProfiler::BeginSample requires an initialized profiler");
    }
    if (!m_frameActive) {
        throw std::runtime_error("D3D11GpuProfiler::BeginSample requires an active frame");
    }

    Sample sample{};
    sample.name = std::move(name);
    sample.timer.Initialize(m_device.Get());
    sample.timer.Begin(context);
    m_samples.push_back(std::move(sample));
    return static_cast<UINT>(m_samples.size() - 1);
}

void D3D11GpuProfiler::EndSample(ID3D11DeviceContext* context, UINT sampleId) {
    RequireProfilerContext(context, "D3D11GpuProfiler::EndSample");
    if (!m_frameActive) {
        throw std::runtime_error("D3D11GpuProfiler::EndSample requires an active frame");
    }
    if (sampleId >= m_samples.size()) {
        throw std::out_of_range("D3D11GpuProfiler::EndSample sample id is invalid");
    }

    auto& sample = m_samples[sampleId];
    if (sample.ended) {
        throw std::runtime_error("D3D11GpuProfiler::EndSample sample has already ended");
    }
    sample.timer.End(context);
    sample.ended = true;
}

std::vector<D3D11GpuProfilerSampleResult> D3D11GpuProfiler::Resolve(
    ID3D11DeviceContext* context,
    bool wait) {
    RequireProfilerContext(context, "D3D11GpuProfiler::Resolve");
    if (!m_initialized) {
        throw std::runtime_error("D3D11GpuProfiler::Resolve requires an initialized profiler");
    }
    if (!m_frameEnded) {
        throw std::runtime_error("D3D11GpuProfiler::Resolve requires an ended frame");
    }

    std::vector<D3D11GpuProfilerSampleResult> results;
    results.reserve(m_samples.size());
    for (auto& sample : m_samples) {
        D3D11GpuProfilerSampleResult sampleResult{};
        sampleResult.name = sample.name;
        const auto timerResult = sample.timer.Resolve(context, wait);
        sampleResult.completed = timerResult.completed;
        sampleResult.valid = timerResult.valid;
        sampleResult.disjoint = timerResult.disjoint;
        sampleResult.milliseconds = timerResult.milliseconds;
        results.push_back(std::move(sampleResult));
    }
    return results;
}

D3D11ScopedGpuProfileSample::D3D11ScopedGpuProfileSample(
    D3D11GpuProfiler& profiler,
    ID3D11DeviceContext* context,
    std::string name)
    : m_profiler(profiler), m_context(context) {
    m_sampleId = m_profiler.BeginSample(m_context, std::move(name));
    m_active = true;
}

D3D11ScopedGpuProfileSample::~D3D11ScopedGpuProfileSample() noexcept {
    if (m_active && m_context) {
        try {
            m_profiler.EndSample(m_context, m_sampleId);
        } catch (...) {
            // Destructors must not throw. Diagnostics users can call EndSample
            // explicitly when they need error reporting.
        }
    }
}

} // namespace D3D11CoreLib
