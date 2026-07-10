//
// D3D11StagingBuffer.cpp
//
#include <D3D11Helper/D3D11Gpu/D3D11StagingBuffer.hpp>
#include <D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp>

#include <limits>
#include <stdexcept>
#include <utility>

namespace D3D11CoreLib {

D3D11MappedSubresource::D3D11MappedSubresource(
    D3D11StagingBuffer* owner,
    ID3D11DeviceContext* context,
    const void* data,
    UINT rowPitch,
    UINT depthPitch) noexcept
    : m_owner(owner),
      m_context(context),
      m_data(data),
      m_rowPitch(rowPitch),
      m_depthPitch(depthPitch) {}

D3D11MappedSubresource::~D3D11MappedSubresource() noexcept {
    Reset();
}

D3D11MappedSubresource::D3D11MappedSubresource(
    D3D11MappedSubresource&& other) noexcept
    : m_owner(std::exchange(other.m_owner, nullptr)),
      m_context(std::exchange(other.m_context, nullptr)),
      m_data(std::exchange(other.m_data, nullptr)),
      m_rowPitch(std::exchange(other.m_rowPitch, 0)),
      m_depthPitch(std::exchange(other.m_depthPitch, 0)) {}

D3D11MappedSubresource& D3D11MappedSubresource::operator=(
    D3D11MappedSubresource&& other) noexcept {
    if (this != &other) {
        Reset();
        m_owner = std::exchange(other.m_owner, nullptr);
        m_context = std::exchange(other.m_context, nullptr);
        m_data = std::exchange(other.m_data, nullptr);
        m_rowPitch = std::exchange(other.m_rowPitch, 0);
        m_depthPitch = std::exchange(other.m_depthPitch, 0);
    }
    return *this;
}

void D3D11MappedSubresource::Reset() noexcept {
    if (m_owner) {
        m_owner->Unmap(m_context);
    }
    m_owner = nullptr;
    m_context = nullptr;
    m_data = nullptr;
    m_rowPitch = 0;
    m_depthPitch = 0;
}

void D3D11StagingBuffer::InitializeAsBuffer(
    ID3D11Device* device,
    UINT64 sizeBytes) {
    if (!device) throw std::runtime_error("D3D11StagingBuffer: null device");
    if (sizeBytes == 0) throw std::runtime_error("D3D11StagingBuffer: size must be > 0");
    if (sizeBytes > static_cast<UINT64>((std::numeric_limits<UINT>::max)())) {
        throw std::runtime_error("D3D11StagingBuffer: size exceeds D3D11 buffer limit");
    }
    if (IsMapped()) {
        throw std::runtime_error("D3D11StagingBuffer: cannot reinitialize while mapped");
    }

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = static_cast<UINT>(sizeBytes);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.BindFlags = 0;

    ComPtr<ID3D11Buffer> buffer;
    D3D11CORE_THROW_IF_FAILED(device->CreateBuffer(&desc, nullptr, &buffer));
    D3D11CORE_THROW_IF_FAILED(buffer.As(&m_resource));
}

void D3D11StagingBuffer::InitializeAsTexture2D(
    ID3D11Device* device,
    UINT width,
    UINT height,
    DXGI_FORMAT format) {
    if (!device) throw std::runtime_error("D3D11StagingBuffer: null device");
    if (width == 0 || height == 0) {
        throw std::runtime_error("D3D11StagingBuffer: texture dimensions must be > 0");
    }
    if (format == DXGI_FORMAT_UNKNOWN) {
        throw std::runtime_error("D3D11StagingBuffer: texture format must be known");
    }
    if (IsMapped()) {
        throw std::runtime_error("D3D11StagingBuffer: cannot reinitialize while mapped");
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.BindFlags = 0;

    ComPtr<ID3D11Texture2D> texture;
    D3D11CORE_THROW_IF_FAILED(device->CreateTexture2D(&desc, nullptr, &texture));
    D3D11CORE_THROW_IF_FAILED(texture.As(&m_resource));
}

const void* D3D11StagingBuffer::Map(ID3D11DeviceContext* ctx) {
    if (!ctx || !m_resource) {
        throw std::runtime_error("D3D11StagingBuffer::Map: not initialized");
    }
    if (m_mappedPtr) return m_mappedPtr;

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    D3D11CORE_THROW_IF_FAILED(
        ctx->Map(m_resource.Get(), 0, D3D11_MAP_READ, 0, &mapped));
    m_mappedPtr = mapped.pData;
    m_mappedRowPitch = mapped.RowPitch;
    m_mappedDepthPitch = mapped.DepthPitch;
    return m_mappedPtr;
}

void D3D11StagingBuffer::Unmap(ID3D11DeviceContext* ctx) {
    if (ctx && m_resource && m_mappedPtr) {
        ctx->Unmap(m_resource.Get(), 0);
        m_mappedPtr = nullptr;
        m_mappedRowPitch = 0;
        m_mappedDepthPitch = 0;
    }
}

D3D11MappedSubresource D3D11StagingBuffer::MapScoped(
    ID3D11DeviceContext* ctx) {
    if (IsMapped()) {
        throw std::runtime_error(
            "D3D11StagingBuffer::MapScoped: resource is already mapped");
    }

    const void* data = Map(ctx);
    return D3D11MappedSubresource(
        this,
        ctx,
        data,
        m_mappedRowPitch,
        m_mappedDepthPitch);
}

} // namespace D3D11CoreLib
