#pragma once
//
// D3D11SharedHandle.hpp - move-only owned HANDLE wrapper for D3D11/DXGI interop.
//
// This helper owns NT handles returned by IDXGIResource1::CreateSharedHandle,
// ID3D11Fence::CreateSharedHandle, DuplicateHandle, or compatible Win32 APIs.
// Raw HANDLE APIs remain available for compatibility, but new code should prefer
// this wrapper at ownership boundaries.
//
#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>

namespace D3D11CoreLib {

class D3D11SharedHandle {
public:
    D3D11SharedHandle() noexcept = default;
    explicit D3D11SharedHandle(HANDLE handle) noexcept;
    ~D3D11SharedHandle();

    D3D11SharedHandle(const D3D11SharedHandle&) = delete;
    D3D11SharedHandle& operator=(const D3D11SharedHandle&) = delete;

    D3D11SharedHandle(D3D11SharedHandle&& other) noexcept;
    D3D11SharedHandle& operator=(D3D11SharedHandle&& other) noexcept;

    bool IsValid() const noexcept;
    explicit operator bool() const noexcept { return IsValid(); }

    HANDLE Get() const noexcept { return m_handle; }
    HANDLE Release() noexcept;
    void Reset(HANDLE handle = nullptr) noexcept;

    D3D11SharedHandle Duplicate(
        DWORD desiredAccess = 0,
        BOOL inheritHandle = FALSE) const;

private:
    static bool IsValidHandle(HANDLE handle) noexcept;

    HANDLE m_handle = nullptr;
};

} // namespace D3D11CoreLib
