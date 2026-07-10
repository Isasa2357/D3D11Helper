//
// D3D11Fence.cpp
//
#include <D3D11Helper/D3D11Interop/D3D11Fence.hpp>
#include <D3D11Helper/D3D11Interop/D3D11FenceSupport.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>

#include <stdexcept>
#include <string>
#include <utility>

namespace D3D11CoreLib {
namespace {

bool IsValidHandle(HANDLE handle) noexcept {
    return handle != nullptr && handle != INVALID_HANDLE_VALUE;
}

ComPtr<ID3D11Device5> RequireDevice5(ID3D11Device* device, const char* functionName) {
    auto device5 = TryGetD3D11Device5(device);
    if (!device5) {
        throw std::runtime_error(std::string(functionName) + ": ID3D11Device5 is unavailable");
    }
    return device5;
}

ComPtr<ID3D11DeviceContext4> RequireContext4(
    ID3D11DeviceContext* context,
    const char* functionName) {
    auto context4 = TryGetD3D11DeviceContext4(context);
    if (!context4) {
        throw std::runtime_error(std::string(functionName) + ": ID3D11DeviceContext4 is unavailable");
    }
    return context4;
}

HANDLE CreateFenceEvent() {
    HANDLE eventHandle = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
    if (eventHandle == nullptr) {
        D3D11CORE_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
    }
    return eventHandle;
}

void WaitForFenceValue(ID3D11Fence* fence, UINT64 value, HANDLE eventHandle) {
    if (!fence || !IsValidHandle(eventHandle)) {
        throw std::runtime_error("WaitForFenceValue: invalid fence or event");
    }
    if (fence->GetCompletedValue() >= value) return;

    D3D11CORE_THROW_IF_FAILED(fence->SetEventOnCompletion(value, eventHandle));
    const DWORD waitResult = WaitForSingleObject(eventHandle, INFINITE);
    if (waitResult != WAIT_OBJECT_0) {
        const DWORD error = waitResult == WAIT_FAILED ? GetLastError() : ERROR_GEN_FAILURE;
        D3D11CORE_THROW_IF_FAILED(HRESULT_FROM_WIN32(error));
    }
}

} // namespace

bool D3D11FencePoint::IsComplete() const noexcept {
    return m_fence && m_fence->GetCompletedValue() >= m_value;
}

void D3D11FencePoint::CpuWait() const {
    if (!m_fence) {
        throw std::runtime_error("D3D11FencePoint::CpuWait: invalid point");
    }
    HANDLE eventHandle = CreateFenceEvent();
    try {
        WaitForFenceValue(m_fence.Get(), m_value, eventHandle);
    } catch (...) {
        CloseHandle(eventHandle);
        throw;
    }
    CloseHandle(eventHandle);
}

D3D11Fence::~D3D11Fence() {
    Destroy();
}

void D3D11Fence::Destroy() noexcept {
    if (m_event) {
        CloseHandle(m_event);
        m_event = nullptr;
    }
    m_fence.Reset();
}

D3D11Fence::D3D11Fence(D3D11Fence&& other) noexcept
    : m_fence(std::move(other.m_fence)), m_event(other.m_event) {
    other.m_event = nullptr;
}

D3D11Fence& D3D11Fence::operator=(D3D11Fence&& other) noexcept {
    if (this != &other) {
        Destroy();
        m_fence = std::move(other.m_fence);
        m_event = other.m_event;
        other.m_event = nullptr;
    }
    return *this;
}

void D3D11Fence::Initialize(ID3D11Device5* device5, D3D11_FENCE_FLAG flags) {
    if (!device5) {
        throw std::runtime_error(
            "D3D11Fence: ID3D11Device5 required (D3D11.4 not available on this adapter)");
    }

    ComPtr<ID3D11Fence> fence;
    D3D11CORE_THROW_IF_FAILED(
        device5->CreateFence(0, flags, IID_PPV_ARGS(&fence)));
    HANDLE eventHandle = CreateFenceEvent();

    Destroy();
    m_fence = std::move(fence);
    m_event = eventHandle;
}

void D3D11Fence::Initialize(ID3D11Device* device, D3D11_FENCE_FLAG flags) {
    auto device5 = RequireDevice5(device, "D3D11Fence::Initialize");
    Initialize(device5.Get(), flags);
}

void D3D11Fence::OpenSharedHandle(ID3D11Device5* device5, HANDLE sharedHandle) {
    if (!device5) throw std::runtime_error("D3D11Fence::OpenSharedHandle: null device5");
    if (!IsValidHandle(sharedHandle)) {
        throw std::invalid_argument("D3D11Fence::OpenSharedHandle: invalid shared handle");
    }

    ComPtr<ID3D11Fence> fence;
    D3D11CORE_THROW_IF_FAILED(
        device5->OpenSharedFence(sharedHandle, IID_PPV_ARGS(&fence)));
    HANDLE eventHandle = CreateFenceEvent();

    Destroy();
    m_fence = std::move(fence);
    m_event = eventHandle;
}

void D3D11Fence::OpenSharedHandle(ID3D11Device* device, HANDLE sharedHandle) {
    auto device5 = RequireDevice5(device, "D3D11Fence::OpenSharedHandle");
    OpenSharedHandle(device5.Get(), sharedHandle);
}

void D3D11Fence::OpenSharedHandle(
    ID3D11Device5* device5,
    const D3D11SharedHandle& sharedHandle) {
    if (!sharedHandle) {
        throw std::invalid_argument("D3D11Fence::OpenSharedHandle: invalid shared handle wrapper");
    }
    OpenSharedHandle(device5, sharedHandle.Get());
}

void D3D11Fence::OpenSharedHandle(
    ID3D11Device* device,
    const D3D11SharedHandle& sharedHandle) {
    if (!sharedHandle) {
        throw std::invalid_argument("D3D11Fence::OpenSharedHandle: invalid shared handle wrapper");
    }
    OpenSharedHandle(device, sharedHandle.Get());
}

void D3D11Fence::Signal(ID3D11DeviceContext4* ctx, UINT64 value) {
    if (!ctx || !m_fence) throw std::runtime_error("D3D11Fence::Signal: not initialized");
    D3D11CORE_THROW_IF_FAILED(ctx->Signal(m_fence.Get(), value));
}

void D3D11Fence::Signal(ID3D11DeviceContext* ctx, UINT64 value) {
    auto ctx4 = RequireContext4(ctx, "D3D11Fence::Signal");
    Signal(ctx4.Get(), value);
}

D3D11FencePoint D3D11Fence::SignalPoint(
    ID3D11DeviceContext* ctx,
    UINT64 value) {
    Signal(ctx, value);
    return D3D11FencePoint(m_fence, value);
}

void D3D11Fence::GpuWait(ID3D11DeviceContext4* ctx, UINT64 value) {
    if (!ctx || !m_fence) throw std::runtime_error("D3D11Fence::GpuWait: not initialized");
    D3D11CORE_THROW_IF_FAILED(ctx->Wait(m_fence.Get(), value));
}

void D3D11Fence::GpuWait(ID3D11DeviceContext* ctx, UINT64 value) {
    auto ctx4 = RequireContext4(ctx, "D3D11Fence::GpuWait");
    GpuWait(ctx4.Get(), value);
}

void D3D11Fence::GpuWaitPoint(
    ID3D11DeviceContext* ctx,
    const D3D11FencePoint& point) {
    if (!point) throw std::invalid_argument("D3D11Fence::GpuWaitPoint: invalid point");
    auto ctx4 = RequireContext4(ctx, "D3D11Fence::GpuWaitPoint");
    D3D11CORE_THROW_IF_FAILED(ctx4->Wait(point.GetFence(), point.GetValue()));
}

void D3D11Fence::CpuWait(UINT64 value) {
    if (!m_fence || !m_event) {
        throw std::runtime_error("D3D11Fence::CpuWait: not initialized");
    }
    WaitForFenceValue(m_fence.Get(), value, m_event);
}

HANDLE D3D11Fence::CreateSharedHandle() const {
    if (!m_fence) {
        throw std::runtime_error("D3D11Fence::CreateSharedHandle: not initialized");
    }
    HANDLE handle = nullptr;
    D3D11CORE_THROW_IF_FAILED(
        m_fence->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr, &handle));
    return handle;
}

D3D11SharedHandle D3D11Fence::CreateSharedHandleOwned() const {
    return D3D11SharedHandle(CreateSharedHandle());
}

UINT64 D3D11Fence::GetCompletedValue() const {
    return m_fence ? m_fence->GetCompletedValue() : 0;
}

} // namespace D3D11CoreLib
