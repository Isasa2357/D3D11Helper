#pragma once
//
// D3D11SharedResource.hpp
// 共有 Handle の作成 / オープン（D3D11/D3D12共有・外部API連携の低レイヤ補助）。
//
// D3D11 でのリソース共有には2つの方法がある:
//   1. IDXGIResource1::CreateSharedHandle  (D3D11.1+, NT Handle)
//   2. IDXGIResource::GetSharedHandle      (レガシー, global handle)
//
// 本クラスは NT Handle（IDXGIResource1）を使う。リソースは
// D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED で
// 作成されている必要がある。
//
// This header belongs to D3D11Interop in the v1.1.0 architecture.
// The legacy path D3D11Core/D3D11SharedResource.hpp remains as a compatibility
// wrapper.
//
#include <D3D11Helper/D3D11Foundation/D3D11Common.hpp>

namespace D3D11CoreLib {

class D3D11SharedResource {
public:
    // NT 共有ハンドルを作成する。
    // resource は D3D11_RESOURCE_MISC_SHARED_NTHANDLE | SHARED で作成されている必要がある。
    // 戻り値の HANDLE は呼び出し側が CloseHandle で解放すること。
    static HANDLE CreateSharedHandle(
        ID3D11Resource* resource,
        LPCWSTR name = nullptr);

    // ID3D11Device1::OpenSharedResource1 で共有リソースを開く。
    static ComPtr<ID3D11Resource> OpenSharedHandle(
        ID3D11Device* device,
        HANDLE handle);

    // Texture2D として開く便利関数。
    static ComPtr<ID3D11Texture2D> OpenSharedTexture2D(
        ID3D11Device* device,
        HANDLE handle);
};

} // namespace D3D11CoreLib
