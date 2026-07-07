//
// D3D11SharedHandle.cpp
//
#include <D3D11Helper/D3D11Interop/D3D11SharedHandle.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>

#include <stdexcept>
#include <utility>

namespace D3D11CoreLib {

namespace {

HRESULT LastErrorHResult() noexcept {
    const DWORD error = GetLastError();
    return error == ERROR_SUCCESS ? E_FAIL : HRESULT_FROM_WIN32(error);
}

} // namespace

D3D11SharedHandle::D3D11SharedHandle(HANDLE handle) noexcept
    : m_handle(handle) {}

D3D11SharedHandle::~D3D11SharedHandle() {
    Reset();
}

D3D11SharedHandle::D3D11SharedHandle(D3D11SharedHandle&& other) noexcept
    : m_handle(other.m_handle) {
    other.m_handle = nullptr;
}

D3D11SharedHandle& D3D11SharedHandle::operator=(D3D11SharedHandle&& other) noexcept {
    if (this != &other) {
        Reset();
        m_handle = other.m_handle;
        other.m_handle = nullptr;
    }
    return *this;
}

bool D3D11SharedHandle::IsValidHandle(HANDLE handle) noexcept {
    return handle != nullptr && handle != INVALID_HANDLE_VALUE;
}

bool D3D11SharedHandle::IsValid() const noexcept {
    return IsValidHandle(m_handle);
}

HANDLE D3D11SharedHandle::Release() noexcept {
    HANDLE handle = m_handle;
    m_handle = nullptr;
    return handle;
}

void D3D11SharedHandle::Reset(HANDLE handle) noexcept {
    if (IsValidHandle(m_handle)) {
        CloseHandle(m_handle);
    }
    m_handle = handle;
}

D3D11SharedHandle D3D11SharedHandle::Duplicate(
    DWORD desiredAccess,
    BOOL inheritHandle) const {

    if (!IsValid()) {
        throw std::runtime_error("D3D11SharedHandle::Duplicate: invalid handle");
    }

    HANDLE duplicate = nullptr;
    const DWORD options = desiredAccess == 0 ? DUPLICATE_SAME_ACCESS : 0;
    const HANDLE process = GetCurrentProcess();

    if (!DuplicateHandle(process,
                         m_handle,
                         process,
                         &duplicate,
                         desiredAccess,
                         inheritHandle,
                         options)) {
        D3D11CORE_THROW_IF_FAILED(LastErrorHResult());
    }

    return D3D11SharedHandle(duplicate);
}

} // namespace D3D11CoreLib
