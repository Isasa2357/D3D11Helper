//
// D3D11GpuTimer.cpp
//
#include <D3D11Helper/D3D11Diagnostics/D3D11GpuTimer.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>

#include <stdexcept>
#include <thread>
#include <string>

namespace D3D11CoreLib {
namespace {

void RequireContext(ID3D11DeviceContext* context, const char* functionName) {
    if (!context) {
        throw std::invalid_argument(std::string(functionName) + " requires a non-null context");
    }
}

bool GetQueryData(ID3D11DeviceContext* context, ID3D11Query* query, void* data, UINT dataSize, bool wait) {
    for (;;) {
        const HRESULT hr = context->GetData(query, data, dataSize, 0);
        if (hr == S_OK) {
            return true;
        }
        if (hr == S_FALSE && !wait) {
            return false;
        }
        if (hr == S_FALSE) {
            std::this_thread::yield();
            continue;
        }
        D3D11CORE_THROW_IF_FAILED(hr);
    }
}

} // namespace

void D3D11GpuTimer::Initialize(ID3D11Device* device) {
    if (!device) {
        throw std::invalid_argument("D3D11GpuTimer::Initialize requires a non-null device");
    }

    D3D11_QUERY_DESC desc{};
    desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
    D3D11CORE_THROW_IF_FAILED(device->CreateQuery(&desc, &m_disjointQuery));

    desc.Query = D3D11_QUERY_TIMESTAMP;
    D3D11CORE_THROW_IF_FAILED(device->CreateQuery(&desc, &m_startQuery));
    D3D11CORE_THROW_IF_FAILED(device->CreateQuery(&desc, &m_endQuery));

    m_initialized = true;
    m_running = false;
    m_ended = false;
}

void D3D11GpuTimer::Reset() noexcept {
    m_disjointQuery.Reset();
    m_startQuery.Reset();
    m_endQuery.Reset();
    m_initialized = false;
    m_running = false;
    m_ended = false;
}

void D3D11GpuTimer::Begin(ID3D11DeviceContext* context) {
    RequireContext(context, "D3D11GpuTimer::Begin");
    if (!m_initialized) {
        throw std::runtime_error("D3D11GpuTimer::Begin requires an initialized timer");
    }
    if (m_running) {
        throw std::runtime_error("D3D11GpuTimer::Begin called while timer is already running");
    }

    context->Begin(m_disjointQuery.Get());
    context->End(m_startQuery.Get());
    m_running = true;
    m_ended = false;
}

void D3D11GpuTimer::End(ID3D11DeviceContext* context) {
    RequireContext(context, "D3D11GpuTimer::End");
    if (!m_running) {
        throw std::runtime_error("D3D11GpuTimer::End requires a running timer");
    }

    context->End(m_endQuery.Get());
    context->End(m_disjointQuery.Get());
    m_running = false;
    m_ended = true;
}

D3D11GpuTimerResult D3D11GpuTimer::Resolve(ID3D11DeviceContext* context, bool wait) {
    RequireContext(context, "D3D11GpuTimer::Resolve");
    if (!m_initialized) {
        throw std::runtime_error("D3D11GpuTimer::Resolve requires an initialized timer");
    }
    if (!m_ended) {
        throw std::runtime_error("D3D11GpuTimer::Resolve requires an ended timer");
    }

    D3D11GpuTimerResult result{};
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
    if (!GetQueryData(context, m_disjointQuery.Get(), &disjoint, sizeof(disjoint), wait)) {
        return result;
    }
    if (!GetQueryData(context, m_startQuery.Get(), &result.startTimestamp, sizeof(result.startTimestamp), wait)) {
        return result;
    }
    if (!GetQueryData(context, m_endQuery.Get(), &result.endTimestamp, sizeof(result.endTimestamp), wait)) {
        return result;
    }

    result.completed = true;
    result.disjoint = disjoint.Disjoint != FALSE;
    result.frequency = disjoint.Frequency;
    if (!result.disjoint && result.frequency != 0 && result.endTimestamp >= result.startTimestamp) {
        result.valid = true;
        result.seconds = static_cast<double>(result.endTimestamp - result.startTimestamp) /
                         static_cast<double>(result.frequency);
        result.milliseconds = result.seconds * 1000.0;
    }
    return result;
}

D3D11ScopedGpuTimer::D3D11ScopedGpuTimer(D3D11GpuTimer& timer, ID3D11DeviceContext* context)
    : m_timer(timer), m_context(context) {
    m_timer.Begin(m_context);
}

D3D11ScopedGpuTimer::~D3D11ScopedGpuTimer() noexcept {
    if (m_context && m_timer.IsRunning()) {
        try {
            m_timer.End(m_context);
        } catch (...) {
            // Destructors must not throw. Diagnostics users can call End/Resolve
            // explicitly when they need error reporting.
        }
    }
}

} // namespace D3D11CoreLib
