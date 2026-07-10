#pragma once
//
// D3D11Fence.hpp
// ID3D11Fence wrapper for D3D11.4 / D3D12 interop.
//
#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>
#include <D3D11Helper/D3D11Interop/D3D11SharedHandle.hpp>

namespace D3D11CoreLib {

class D3D11Fence;

// Immutable fence + value pair. The ComPtr keeps the fence alive independently
// from the D3D11Fence wrapper that created the point.
class D3D11FencePoint {
public:
    D3D11FencePoint() noexcept = default;

    ID3D11Fence* GetFence() const noexcept { return m_fence.Get(); }
    UINT64 GetValue() const noexcept { return m_value; }
    bool IsValid() const noexcept { return m_fence != nullptr; }
    explicit operator bool() const noexcept { return IsValid(); }

    bool IsComplete() const noexcept;
    void CpuWait() const;

private:
    friend class D3D11Fence;

    D3D11FencePoint(ComPtr<ID3D11Fence> fence, UINT64 value) noexcept
        : m_fence(std::move(fence)), m_value(value) {}

    ComPtr<ID3D11Fence> m_fence;
    UINT64 m_value = 0;
};

class D3D11Fence {
public:
    D3D11Fence() = default;
    ~D3D11Fence();

    D3D11Fence(const D3D11Fence&) = delete;
    D3D11Fence& operator=(const D3D11Fence&) = delete;
    D3D11Fence(D3D11Fence&& other) noexcept;
    D3D11Fence& operator=(D3D11Fence&& other) noexcept;

    void Initialize(
        ID3D11Device5* device5,
        D3D11_FENCE_FLAG flags = D3D11_FENCE_FLAG_SHARED);
    void Initialize(
        ID3D11Device* device,
        D3D11_FENCE_FLAG flags = D3D11_FENCE_FLAG_SHARED);

    void OpenSharedHandle(ID3D11Device5* device5, HANDLE sharedHandle);
    void OpenSharedHandle(ID3D11Device* device, HANDLE sharedHandle);
    void OpenSharedHandle(ID3D11Device5* device5, const D3D11SharedHandle& sharedHandle);
    void OpenSharedHandle(ID3D11Device* device, const D3D11SharedHandle& sharedHandle);

    void Signal(ID3D11DeviceContext4* ctx, UINT64 value);
    void Signal(ID3D11DeviceContext* ctx, UINT64 value);

    // Separately named point API to avoid adding another same-name overload to
    // the existing Signal overload set.
    D3D11FencePoint SignalPoint(ID3D11DeviceContext* ctx, UINT64 value);

    // Important: an Immediate Context has one ordered GPU timeline. Do not queue
    // a wait for a point that only a later command on the same context can signal.
    void GpuWait(ID3D11DeviceContext4* ctx, UINT64 value);
    void GpuWait(ID3D11DeviceContext* ctx, UINT64 value);

    // Waits for the fence carried by point. This supports points created by a
    // separately opened/shared fence as well as this wrapper's own fence.
    void GpuWaitPoint(ID3D11DeviceContext* ctx, const D3D11FencePoint& point);

    void CpuWait(UINT64 value);

    HANDLE CreateSharedHandle() const;
    D3D11SharedHandle CreateSharedHandleOwned() const;

    UINT64 GetCompletedValue() const;
    ID3D11Fence* Get() const noexcept { return m_fence.Get(); }
    bool IsInitialized() const noexcept { return m_fence != nullptr; }

private:
    void Destroy() noexcept;

    ComPtr<ID3D11Fence> m_fence;
    HANDLE m_event = nullptr;
};

} // namespace D3D11CoreLib
