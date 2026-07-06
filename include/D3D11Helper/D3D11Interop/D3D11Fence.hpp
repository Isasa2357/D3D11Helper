#pragma once
//
// D3D11Fence.hpp
// ID3D11Fence ラッパ（D3D11.4 / D3D12 相互運用向け）。
//
// D3D12 の D3D12Fence に相当するが、D3D11 では GPU 同期の主手段は Flush() と
// ID3D11Query(EVENT) である。このクラスは D3D11/D3D12 相互運用（SharedResource 経由の
// Fence 同期）に使う。ID3D11Device5 が取得できない環境では Initialize が例外を投げる。
//
// HANDLE を所有するため move-only。
//
// This header belongs to D3D11Interop in the v1.1.0 architecture.
// The legacy path D3D11Core/D3D11Fence.hpp remains as a compatibility wrapper.
//
#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>

namespace D3D11CoreLib {

class D3D11Fence {
public:
    D3D11Fence() = default;
    ~D3D11Fence();

    D3D11Fence(const D3D11Fence&)            = delete;
    D3D11Fence& operator=(const D3D11Fence&) = delete;
    D3D11Fence(D3D11Fence&& other) noexcept;
    D3D11Fence& operator=(D3D11Fence&& other) noexcept;

    // D3D11.4 の ID3D11Fence を作成する。
    void Initialize(ID3D11Device5* device5,
                    D3D11_FENCE_FLAG flags = D3D11_FENCE_FLAG_SHARED);

    // 共有ハンドルからオープンする（D3D12 の Fence と同期する場合）。
    void OpenSharedHandle(ID3D11Device5* device5, HANDLE sharedHandle);

    // GPU に Signal をキューイングする。
    void Signal(ID3D11DeviceContext4* ctx, UINT64 value);

    // GPU 側で Fence 値を待つ。
    //
    // 注意（重要）: D3D11 の Immediate Context は単一の GPU タイムラインしか持たない。
    // 同じコンテキストに対して GpuWait(N) を積んだ「後」に、その値を満たす Signal(N) を
    // 積んでも、GPU はコマンドを投入順に処理するため GpuWait で永久に停止し、
    // 後続の Signal に到達できない（自己デッドロック）。
    // GpuWait は「別のデバイス/コンテキストが将来 Signal する値」を待つ用途で使うこと
    // （例: D3D12 との相互運用、または複数の D3D11Core 間での同期）。
    //
    // また、WARP（ソフトウェアアダプタ）では Fence の Wait/Signal がハードウェアほど
    // 安定して動作しない場合がある。WARP を使う可能性がある場合は、GpuWait に依存せず
    // Signal + CpuWait による CPU 側同期を優先することを推奨する。
    void GpuWait(ID3D11DeviceContext4* ctx, UINT64 value);

    // CPU 側で Fence 値の完了をブロック待ちする。
    void CpuWait(UINT64 value);

    // Fence の共有ハンドルを作成する（D3D12 側でオープンするため）。
    HANDLE CreateSharedHandle() const;

    UINT64       GetCompletedValue() const;
    ID3D11Fence* Get() const noexcept { return m_fence.Get(); }

private:
    void Destroy() noexcept;

    ComPtr<ID3D11Fence> m_fence;
    HANDLE              m_event = nullptr;
};

} // namespace D3D11CoreLib
