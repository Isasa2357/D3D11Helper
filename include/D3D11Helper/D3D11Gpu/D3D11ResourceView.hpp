#pragma once
//
// D3D11ResourceView.hpp
// External-owner ID3D11Resource view with no AddRef / Release behavior.
//
#include <D3D11Helper/D3D11Gpu/D3D11Resource.hpp>

namespace D3D11CoreLib {

// A pointer-sized, non-owning resource view.
//
// Construction, copy, assignment, and destruction never call AddRef or Release.
// The external owner must keep the resource alive for every operation that uses
// the view. Processing entry points may create temporary SRV/UAV COM objects;
// those objects follow the normal D3D11 ownership rules and are released before
// an Immediate-Context dispatch call returns.
class D3D11ResourceView {
public:
    D3D11ResourceView() noexcept = default;

    explicit D3D11ResourceView(ID3D11Resource* resource) noexcept
        : m_resource(resource) {}

    explicit D3D11ResourceView(ID3D11Texture2D* texture) noexcept
        : m_resource(static_cast<ID3D11Resource*>(texture)) {}

    explicit D3D11ResourceView(ID3D11Buffer* buffer) noexcept
        : m_resource(static_cast<ID3D11Resource*>(buffer)) {}

    explicit D3D11ResourceView(const D3D11Resource& resource) noexcept
        : m_resource(resource.Get()) {}

    ID3D11Resource* Get() const noexcept { return m_resource; }
    explicit operator bool() const noexcept { return m_resource != nullptr; }

private:
    ID3D11Resource* m_resource = nullptr;
};

namespace Detail {

// Internal adapter used only while delegating to established owned-resource
// implementations. Attach does not AddRef. The destructor clears any temporary
// queried interfaces, then Detach prevents the borrowed base pointer from being
// released by D3D11Resource's ComPtr member.
class D3D11ScopedResourceViewAdapter final {
public:
    explicit D3D11ScopedResourceViewAdapter(D3D11ResourceView view) noexcept {
        m_resource.m_resource.Attach(view.Get());
    }

    ~D3D11ScopedResourceViewAdapter() noexcept {
        Reset();
    }

    D3D11ScopedResourceViewAdapter(const D3D11ScopedResourceViewAdapter&) = delete;
    D3D11ScopedResourceViewAdapter& operator=(const D3D11ScopedResourceViewAdapter&) = delete;
    D3D11ScopedResourceViewAdapter(D3D11ScopedResourceViewAdapter&&) = delete;
    D3D11ScopedResourceViewAdapter& operator=(D3D11ScopedResourceViewAdapter&&) = delete;

    D3D11Resource& Resource() noexcept { return m_resource; }
    const D3D11Resource& Resource() const noexcept { return m_resource; }

private:
    void Reset() noexcept {
        m_resource.m_texture2D.Reset();
        m_resource.m_buffer.Reset();
        m_resource.m_resource.Detach();
    }

    D3D11Resource m_resource;
};

} // namespace Detail
} // namespace D3D11CoreLib
