# D3D11Interop

`D3D11Interop` は、D3D11/D3D12 共有や外部 API 連携の入口になる DirectX/DXGI レベルの共有機能を扱います。

個別外部 API への依存はこのモジュールには入れません。

## Include

```cpp
#include <D3D11Helper/D3D11Interop/D3D11Interop.hpp>
```

## D3D11SharedResource

```cpp
static HANDLE CreateSharedHandle(ID3D11Resource* resource, LPCWSTR name = nullptr);
static ComPtr<ID3D11Resource> OpenSharedHandle(ID3D11Device* device, HANDLE handle);
static ComPtr<ID3D11Texture2D> OpenSharedTexture2D(ID3D11Device* device, HANDLE handle);
```

NT shared handle を使った共有リソース補助です。

## D3D11Fence

D3D11.4 の `ID3D11Fence` wrapper です。主に D3D12 との interop で使います。

```cpp
void Initialize(ID3D11Device5* device5, D3D11_FENCE_FLAG flags = D3D11_FENCE_FLAG_SHARED);
void OpenSharedHandle(ID3D11Device5* device5, HANDLE sharedHandle);
void Signal(ID3D11DeviceContext4* ctx, UINT64 value);
void GpuWait(ID3D11DeviceContext4* ctx, UINT64 value);
void CpuWait(UINT64 value);
HANDLE CreateSharedHandle() const;
UINT64 GetCompletedValue() const;
```

## 互換パス

旧パスも v1.x では維持されます。

```cpp
#include <D3D11Helper/D3D11Core/D3D11Fence.hpp>
#include <D3D11Helper/D3D11Core/D3D11SharedResource.hpp>
```
