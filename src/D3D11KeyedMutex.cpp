//
// D3D11KeyedMutex.cpp
//
#include <D3D11Helper/D3D11Interop/D3D11KeyedMutex.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>

#include <stdexcept>
#include <string>
#include <utility>

namespace D3D11CoreLib {
namespace {

bool IsTimeoutResult(HRESULT hr) noexcept {
    return hr == static_cast<HRESULT>(WAIT_TIMEOUT) ||
           hr == HRESULT_FROM_WIN32(WAIT_TIMEOUT);
}

std::runtime_error MakeTimeoutError(const char* functionName) {
    return std::runtime_error(std::string(functionName) + ": keyed mutex acquire timed out");
}

} // namespace

ComPtr<IDXGIKeyedMutex> GetKeyedMutex(ID3D11Resource* resource) {
    if (!resource) {
        throw std::invalid_argument("GetKeyedMutex requires a non-null resource");
    }

    ComPtr<IDXGIKeyedMutex> mutex;
    const HRESULT hr = resource->QueryInterface(IID_PPV_ARGS(&mutex));
    if (FAILED(hr)) {
        if (hr == E_NOINTERFACE) {
            throw std::invalid_argument("GetKeyedMutex: resource does not expose IDXGIKeyedMutex");
        }
        D3D11CORE_THROW_IF_FAILED(hr);
    }
    return mutex;
}

bool HasKeyedMutex(ID3D11Resource* resource) noexcept {
    if (!resource) {
        return false;
    }
    ComPtr<IDXGIKeyedMutex> mutex;
    return SUCCEEDED(resource->QueryInterface(IID_PPV_ARGS(&mutex))) && mutex != nullptr;
}

D3D11KeyedMutex::D3D11KeyedMutex(ID3D11Resource* resource) {
    Attach(resource);
}

D3D11KeyedMutex::D3D11KeyedMutex(D3D11KeyedMutex&& other) noexcept {
    MoveFrom(std::move(other));
}

D3D11KeyedMutex& D3D11KeyedMutex::operator=(D3D11KeyedMutex&& other) noexcept {
    if (this != &other) {
        m_mutex.Reset();
        m_acquired = false;
        MoveFrom(std::move(other));
    }
    return *this;
}

void D3D11KeyedMutex::MoveFrom(D3D11KeyedMutex&& other) noexcept {
    m_mutex = std::move(other.m_mutex);
    m_acquired = other.m_acquired;
    other.m_acquired = false;
}

void D3D11KeyedMutex::Attach(ID3D11Resource* resource) {
    if (m_acquired) {
        throw std::runtime_error("D3D11KeyedMutex::Attach cannot replace a currently-acquired mutex");
    }
    m_mutex = GetKeyedMutex(resource);
    m_acquired = false;
}

void D3D11KeyedMutex::Reset() {
    if (m_acquired) {
        throw std::runtime_error("D3D11KeyedMutex::Reset cannot detach a currently-acquired mutex");
    }
    m_mutex.Reset();
}

void D3D11KeyedMutex::RequireAttached(const char* functionName) const {
    if (!m_mutex) {
        throw std::runtime_error(std::string(functionName) + " requires an attached IDXGIKeyedMutex");
    }
}

bool D3D11KeyedMutex::Acquire(UINT64 key, DWORD timeoutMs) {
    RequireAttached("D3D11KeyedMutex::Acquire");
    if (m_acquired) {
        throw std::runtime_error("D3D11KeyedMutex::Acquire called while this wrapper already owns the mutex");
    }

    const HRESULT hr = m_mutex->AcquireSync(key, timeoutMs);
    if (hr == S_OK) {
        m_acquired = true;
        return true;
    }
    if (IsTimeoutResult(hr)) {
        return false;
    }
    D3D11CORE_THROW_IF_FAILED(hr);
    return false;
}

void D3D11KeyedMutex::AcquireOrThrow(UINT64 key, DWORD timeoutMs) {
    if (!Acquire(key, timeoutMs)) {
        throw MakeTimeoutError("D3D11KeyedMutex::AcquireOrThrow");
    }
}

void D3D11KeyedMutex::Release(UINT64 key) {
    RequireAttached("D3D11KeyedMutex::Release");
    if (!m_acquired) {
        throw std::runtime_error("D3D11KeyedMutex::Release called while this wrapper does not own the mutex");
    }

    D3D11CORE_THROW_IF_FAILED(m_mutex->ReleaseSync(key));
    m_acquired = false;
}

D3D11ScopedKeyedMutexAcquire::D3D11ScopedKeyedMutexAcquire(
    D3D11KeyedMutex& mutex,
    UINT64 acquireKey,
    UINT64 releaseKey,
    DWORD timeoutMs)
    : m_mutex(&mutex), m_releaseKey(releaseKey) {
    m_acquired = m_mutex->Acquire(acquireKey, timeoutMs);
    if (!m_acquired) {
        m_mutex = nullptr;
        throw MakeTimeoutError("D3D11ScopedKeyedMutexAcquire");
    }
}

D3D11ScopedKeyedMutexAcquire::~D3D11ScopedKeyedMutexAcquire() noexcept {
    ReleaseNoThrow();
}

D3D11ScopedKeyedMutexAcquire::D3D11ScopedKeyedMutexAcquire(
    D3D11ScopedKeyedMutexAcquire&& other) noexcept {
    MoveFrom(std::move(other));
}

D3D11ScopedKeyedMutexAcquire& D3D11ScopedKeyedMutexAcquire::operator=(
    D3D11ScopedKeyedMutexAcquire&& other) noexcept {
    if (this != &other) {
        ReleaseNoThrow();
        MoveFrom(std::move(other));
    }
    return *this;
}

void D3D11ScopedKeyedMutexAcquire::MoveFrom(
    D3D11ScopedKeyedMutexAcquire&& other) noexcept {
    m_mutex = other.m_mutex;
    m_releaseKey = other.m_releaseKey;
    m_acquired = other.m_acquired;

    other.m_mutex = nullptr;
    other.m_acquired = false;
    other.m_releaseKey = 0;
}

void D3D11ScopedKeyedMutexAcquire::ReleaseNow() {
    if (m_acquired && m_mutex) {
        m_mutex->Release(m_releaseKey);
        m_acquired = false;
        m_mutex = nullptr;
    }
}

void D3D11ScopedKeyedMutexAcquire::ReleaseNoThrow() noexcept {
    if (m_acquired && m_mutex) {
        try {
            m_mutex->Release(m_releaseKey);
        } catch (...) {
        }
    }
    m_acquired = false;
    m_mutex = nullptr;
}

} // namespace D3D11CoreLib
