#pragma once
//
// D3D11GpuTimer.hpp - synchronous GPU timestamp timer
//
#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>

namespace D3D11CoreLib {

struct D3D11GpuTimerResult {
    bool completed = false;
    bool valid = false;
    bool disjoint = false;

    UINT64 startTimestamp = 0;
    UINT64 endTimestamp = 0;
    UINT64 frequency = 0;

    double seconds = 0.0;
    double milliseconds = 0.0;
};

class D3D11GpuTimer {
public:
    void Initialize(ID3D11Device* device);
    void Reset() noexcept;

    bool IsInitialized() const noexcept { return m_initialized; }
    bool IsRunning() const noexcept { return m_running; }
    bool HasEnded() const noexcept { return m_ended; }

    void Begin(ID3D11DeviceContext* context);
    void End(ID3D11DeviceContext* context);

    D3D11GpuTimerResult Resolve(ID3D11DeviceContext* context, bool wait = true);

private:
    ComPtr<ID3D11Query> m_disjointQuery;
    ComPtr<ID3D11Query> m_startQuery;
    ComPtr<ID3D11Query> m_endQuery;
    bool m_initialized = false;
    bool m_running = false;
    bool m_ended = false;
};

class D3D11ScopedGpuTimer {
public:
    D3D11ScopedGpuTimer(D3D11GpuTimer& timer, ID3D11DeviceContext* context);
    ~D3D11ScopedGpuTimer() noexcept;

    D3D11ScopedGpuTimer(const D3D11ScopedGpuTimer&) = delete;
    D3D11ScopedGpuTimer& operator=(const D3D11ScopedGpuTimer&) = delete;

private:
    D3D11GpuTimer& m_timer;
    ID3D11DeviceContext* m_context = nullptr;
};

} // namespace D3D11CoreLib
