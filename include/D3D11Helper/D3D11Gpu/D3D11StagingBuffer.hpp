#pragma once
//
// D3D11StagingBuffer.hpp
// GPU→CPU 読み戻し用のステージングバッファ。
//
// D3D12 の ReadbackBuffer に相当する。D3D11 では D3D11_USAGE_STAGING +
// D3D11_CPU_ACCESS_READ で作成したバッファを Map/Unmap して読み出す。
//
// D3D12 と異なり D3D11 では Map/Unmap に DeviceContext が必要なため、
// Map/Unmap の引数に context を取る。
//
#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>

namespace D3D11CoreLib {

class D3D11StagingBuffer {
public:
    D3D11StagingBuffer() = default;
    ~D3D11StagingBuffer() = default;

    D3D11StagingBuffer(const D3D11StagingBuffer&)            = delete;
    D3D11StagingBuffer& operator=(const D3D11StagingBuffer&) = delete;
    D3D11StagingBuffer(D3D11StagingBuffer&&)                 = default;
    D3D11StagingBuffer& operator=(D3D11StagingBuffer&&)      = default;

    // 線形バッファとして初期化（構造化データの読み戻し用）。
    void InitializeAsBuffer(ID3D11Device* device, UINT64 sizeBytes);

    // Texture2D と同じサイズ・フォーマットのステージングテクスチャとして初期化。
    // CopyResource / CopySubresourceRegion のコピー先として使う。
    void InitializeAsTexture2D(ID3D11Device* device,
                               UINT width, UINT height, DXGI_FORMAT format);

    // GPU 処理完了後に呼ぶこと。Map して CPU ポインタを返す。
    const void* Map(ID3D11DeviceContext* ctx);
    void        Unmap(ID3D11DeviceContext* ctx);

    // Map 時の行ピッチを返す（Texture2D ステージング時に必要）。
    UINT GetMappedRowPitch() const noexcept { return m_mappedRowPitch; }

    ID3D11Resource* Get() const noexcept { return m_resource.Get(); }

private:
    ComPtr<ID3D11Resource> m_resource;
    const void* m_mappedPtr      = nullptr;
    UINT        m_mappedRowPitch = 0;
};

} // namespace D3D11CoreLib
