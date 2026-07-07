#pragma once
//
// D3D11KeyedMutex.hpp - IDXGIKeyedMutex wrapper for shared D3D11 resources.
//
// Keyed mutex synchronization is useful for resources created with
// D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX. The wrapper intentionally does not
// hide the key protocol. Callers must still design the acquire/release key
// sequence shared by all producers and consumers.
//
#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>

namespace D3D11CoreLib {

ComPtr<IDXGIKeyedMutex> GetKeyedMutex(ID3D11Resource* resource);
bool HasKeyedMutex(ID3D11Resource* resource) noexcept;

class D3D11KeyedMutex {
public:
    D3D11KeyedMutex() = default;
    explicit D3D11KeyedMutex(ID3D11Resource* resource);

    D3D11KeyedMutex(const D3D11KeyedMutex&) = delete;
    D3D11KeyedMutex& operator=(const D3D11KeyedMutex&) = delete;

    D3D11KeyedMutex(D3D11KeyedMutex&& other) noexcept;
    D3D11KeyedMutex& operator=(D3D11KeyedMutex&& other) noexcept;

    void Attach(ID3D11Resource* resource);
    void Reset();

    bool IsAttached() const noexcept { return m_mutex != nullptr; }
    bool IsAcquired() const noexcept { return m_acquired; }
    explicit operator bool() const noexcept { return IsAttached(); }

    // Returns false on timeout. Other failures throw.
    bool Acquire(UINT64 key, DWORD timeoutMs = INFINITE);

    // Throws on timeout or other failures.
    void AcquireOrThrow(UINT64 key, DWORD timeoutMs = INFINITE);

    // Releases a mutex acquired by this wrapper. Throws on failure.
    void Release(UINT64 key);

    IDXGIKeyedMutex* Get() const noexcept { return m_mutex.Get(); }

private:
    void MoveFrom(D3D11KeyedMutex&& other) noexcept;
    void RequireAttached(const char* functionName) const;

    ComPtr<IDXGIKeyedMutex> m_mutex;
    bool m_acquired = false;
};

class D3D11ScopedKeyedMutexAcquire {
public:
    D3D11ScopedKeyedMutexAcquire(
        D3D11KeyedMutex& mutex,
        UINT64 acquireKey,
        UINT64 releaseKey,
        DWORD timeoutMs = INFINITE);

    ~D3D11ScopedKeyedMutexAcquire() noexcept;

    D3D11ScopedKeyedMutexAcquire(const D3D11ScopedKeyedMutexAcquire&) = delete;
    D3D11ScopedKeyedMutexAcquire& operator=(const D3D11ScopedKeyedMutexAcquire&) = delete;

    D3D11ScopedKeyedMutexAcquire(D3D11ScopedKeyedMutexAcquire&& other) noexcept;
    D3D11ScopedKeyedMutexAcquire& operator=(D3D11ScopedKeyedMutexAcquire&& other) noexcept;

    bool Acquired() const noexcept { return m_acquired; }
    UINT64 ReleaseKey() const noexcept { return m_releaseKey; }

    // Releases early if currently acquired. Safe to call more than once.
    void ReleaseNow();

private:
    void ReleaseNoThrow() noexcept;
    void MoveFrom(D3D11ScopedKeyedMutexAcquire&& other) noexcept;

    D3D11KeyedMutex* m_mutex = nullptr;
    UINT64 m_releaseKey = 0;
    bool m_acquired = false;
};

} // namespace D3D11CoreLib
