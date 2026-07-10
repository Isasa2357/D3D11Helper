#pragma once
//
// D3D11StagingBuffer.hpp
// GPU to CPU staging resource with manual and scoped Map APIs.
//
#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>

#include <cstddef>

namespace D3D11CoreLib {

class D3D11StagingBuffer;

// Move-only RAII guard for one mapped staging subresource.
//
// The source D3D11StagingBuffer and ID3D11DeviceContext must outlive the guard.
// Destruction calls D3D11StagingBuffer::Unmap().
class D3D11MappedSubresource {
public:
    D3D11MappedSubresource() noexcept = default;
    ~D3D11MappedSubresource() noexcept;

    D3D11MappedSubresource(const D3D11MappedSubresource&) = delete;
    D3D11MappedSubresource& operator=(const D3D11MappedSubresource&) = delete;

    D3D11MappedSubresource(D3D11MappedSubresource&& other) noexcept;
    D3D11MappedSubresource& operator=(D3D11MappedSubresource&& other) noexcept;

    const void* Data() const noexcept { return m_data; }
    template<class T>
    const T* DataAs() const noexcept { return static_cast<const T*>(m_data); }

    UINT RowPitch() const noexcept { return m_rowPitch; }
    UINT DepthPitch() const noexcept { return m_depthPitch; }
    explicit operator bool() const noexcept { return m_data != nullptr; }

    void Reset() noexcept;

private:
    friend class D3D11StagingBuffer;

    D3D11MappedSubresource(
        D3D11StagingBuffer* owner,
        ID3D11DeviceContext* context,
        const void* data,
        UINT rowPitch,
        UINT depthPitch) noexcept;

    D3D11StagingBuffer* m_owner = nullptr;
    ID3D11DeviceContext* m_context = nullptr;
    const void* m_data = nullptr;
    UINT m_rowPitch = 0;
    UINT m_depthPitch = 0;
};

class D3D11StagingBuffer {
public:
    D3D11StagingBuffer() = default;
    ~D3D11StagingBuffer() = default;

    D3D11StagingBuffer(const D3D11StagingBuffer&) = delete;
    D3D11StagingBuffer& operator=(const D3D11StagingBuffer&) = delete;
    D3D11StagingBuffer(D3D11StagingBuffer&&) = default;
    D3D11StagingBuffer& operator=(D3D11StagingBuffer&&) = default;

    void InitializeAsBuffer(ID3D11Device* device, UINT64 sizeBytes);
    void InitializeAsTexture2D(
        ID3D11Device* device,
        UINT width,
        UINT height,
        DXGI_FORMAT format);

    // Legacy manual mapping API. Existing behavior is preserved: calling Map
    // again while already mapped returns the same pointer.
    const void* Map(ID3D11DeviceContext* ctx);
    void Unmap(ID3D11DeviceContext* ctx);

    // Scoped mapping API. Unlike the legacy Map API, this rejects an already
    // mapped resource so that two guards can never own the same Unmap action.
    D3D11MappedSubresource MapScoped(ID3D11DeviceContext* ctx);

    UINT GetMappedRowPitch() const noexcept { return m_mappedRowPitch; }
    UINT GetMappedDepthPitch() const noexcept { return m_mappedDepthPitch; }
    bool IsMapped() const noexcept { return m_mappedPtr != nullptr; }

    ID3D11Resource* Get() const noexcept { return m_resource.Get(); }

private:
    ComPtr<ID3D11Resource> m_resource;
    const void* m_mappedPtr = nullptr;
    UINT m_mappedRowPitch = 0;
    UINT m_mappedDepthPitch = 0;
};

} // namespace D3D11CoreLib
