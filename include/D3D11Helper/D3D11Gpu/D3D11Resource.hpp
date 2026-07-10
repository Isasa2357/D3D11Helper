#pragma once
//
// D3D11Resource.hpp
// ID3D11Resource の薄い所有ラッパ。
//
// D3D12 との違い:
//   D3D11 はドライバが状態遷移を自動管理するため、Resource State の追跡は不要。
//   本クラスは ID3D11Resource（または Texture2D / Buffer）を ComPtr で所有する。
//
#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>

#include <utility>

namespace D3D11CoreLib {

namespace Detail {
class D3D11ScopedResourceViewAdapter;
}

class D3D11Resource {
public:
    D3D11Resource() = default;

    explicit D3D11Resource(ComPtr<ID3D11Resource> resource)
        : m_resource(std::move(resource)) {}

    D3D11Resource(ComPtr<ID3D11Texture2D> texture)
        : m_texture2D(std::move(texture)) {
        if (m_texture2D) m_texture2D.As(&m_resource);
    }

    D3D11Resource(ComPtr<ID3D11Buffer> buffer)
        : m_buffer(std::move(buffer)) {
        if (m_buffer) m_buffer.As(&m_resource);
    }

    ID3D11Resource* Get() const noexcept { return m_resource.Get(); }
    explicit operator bool() const noexcept { return m_resource != nullptr; }

    // Texture2D / Buffer として取得する。キャッシュ済みの場合はそのまま返す。
    ID3D11Texture2D* AsTexture2D() {
        if (!m_texture2D && m_resource) m_resource.As(&m_texture2D);
        return m_texture2D.Get();
    }

    ID3D11Buffer* AsBuffer() {
        if (!m_buffer && m_resource) m_resource.As(&m_buffer);
        return m_buffer.Get();
    }

    D3D11_TEXTURE2D_DESC GetTexture2DDesc() const {
        D3D11_TEXTURE2D_DESC desc = {};
        ComPtr<ID3D11Texture2D> tex;
        if (m_texture2D) {
            m_texture2D->GetDesc(&desc);
        } else if (m_resource && SUCCEEDED(m_resource.As(&tex))) {
            tex->GetDesc(&desc);
        }
        return desc;
    }

    D3D11_BUFFER_DESC GetBufferDesc() const {
        D3D11_BUFFER_DESC desc = {};
        ComPtr<ID3D11Buffer> buf;
        if (m_buffer) {
            m_buffer->GetDesc(&desc);
        } else if (m_resource && SUCCEEDED(m_resource.As(&buf))) {
            buf->GetDesc(&desc);
        }
        return desc;
    }

    UINT GetWidth() const {
        return GetTexture2DDesc().Width;
    }

    UINT GetHeight() const {
        return GetTexture2DDesc().Height;
    }

    DXGI_FORMAT GetFormat() const {
        return GetTexture2DDesc().Format;
    }

private:
    friend class Detail::D3D11ScopedResourceViewAdapter;

    ComPtr<ID3D11Resource>  m_resource;
    ComPtr<ID3D11Texture2D> m_texture2D; // キャッシュ（取得済みなら保持）
    ComPtr<ID3D11Buffer>    m_buffer;    // キャッシュ
};

} // namespace D3D11CoreLib
