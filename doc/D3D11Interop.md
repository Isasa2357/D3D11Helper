# D3D11Interop

`D3D11Interop` は、D3D11/D3D12 共有や外部 API 連携の入口になる DirectX/DXGI レベルの共有機能を扱います。

個別外部 API への依存はこのモジュールには入れません。CUDA、Varjo、NVENC、Media Foundation、OpenCV などとの統合は上位ライブラリで扱います。

## Include

```cpp
#include <D3D11Helper/D3D11Interop/D3D11Interop.hpp>
```

---

## D3D11SharedHandle

`D3D11SharedHandle` は、NT shared handle を所有する move-only wrapper です。

```cpp
D3D11SharedHandle handle(rawHandle);

if (handle) {
    HANDLE borrowed = handle.Get();
}

D3D11SharedHandle duplicate = handle.Duplicate();
HANDLE raw = duplicate.Release(); // caller now owns raw
CloseHandle(raw);
```

主な用途:

```text
- IDXGIResource1::CreateSharedHandle が返す HANDLE の所有
- ID3D11Fence::CreateSharedHandle が返す HANDLE の所有
- DuplicateHandle で複製した HANDLE の所有
- CloseHandle 漏れの防止
```

Raw `HANDLE` を返す既存 API は互換のため維持します。新規コードでは所有境界に `D3D11SharedHandle` を使うことを推奨します。

---

## D3D11SharedResource

既存の shared resource helper です。

```cpp
static HANDLE CreateSharedHandle(ID3D11Resource* resource, LPCWSTR name = nullptr);
static D3D11SharedHandle CreateSharedHandleOwned(ID3D11Resource* resource, LPCWSTR name = nullptr);
static ComPtr<ID3D11Resource> OpenSharedHandle(ID3D11Device* device, HANDLE handle);
static ComPtr<ID3D11Texture2D> OpenSharedTexture2D(ID3D11Device* device, HANDLE handle);
```

`CreateSharedHandle` は raw `HANDLE` を返します。呼び出し側が `CloseHandle` で閉じる必要があります。
`CreateSharedHandleOwned` は `D3D11SharedHandle` を返します。

共有対象 resource は `D3D11_RESOURCE_MISC_SHARED_NTHANDLE` を含む misc flag で作成されている必要があります。

---

## D3D11SharedTexture

v1.8.0 以降では、shared `Texture2D` 作成と open の helper を使えます。

```cpp
D3D11SharedTexture2DDesc desc = {};
desc.width = 1280;
desc.height = 720;
desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
desc.bindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
desc.syncMode = D3D11SharedTextureSyncMode::SharedFence;

D3D11SharedTexture2D shared = CreateSharedTexture2DWithHandle(core->GetDevice(), desc);
ComPtr<ID3D11Texture2D> opened = OpenSharedTexture2D(core->GetDevice(), shared.handle);
```

`D3D11SharedTextureSyncMode` は共有 texture の同期方式を指定します。

```cpp
enum class D3D11SharedTextureSyncMode {
    SharedFence,
    KeyedMutex
};
```

`SharedFence` は `D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED` を使います。D3D11.4 fence や外部 timeline 同期と組み合わせる用途です。

`KeyedMutex` は `D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX` を使います。`IDXGIKeyedMutex` による acquire/release key protocol を使う用途です。

`D3D11SharedTexture2DDesc::miscFlags` には共有関連 flag を直接入れないでください。共有 flag は `syncMode` から決まります。

---

## D3D11KeyedMutex

`D3D11KeyedMutex` は `IDXGIKeyedMutex` を薄く包む helper です。

```cpp
D3D11SharedTexture2DDesc desc = {};
desc.width = 640;
desc.height = 480;
desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
desc.syncMode = D3D11SharedTextureSyncMode::KeyedMutex;

auto shared = CreateSharedTexture2DWithHandle(core->GetDevice(), desc);

D3D11KeyedMutex mutex(shared.Get());
mutex.AcquireOrThrow(0, 1000);
// use texture while key 0 is acquired
mutex.Release(1);

D3D11ScopedKeyedMutexAcquire scoped(mutex, 1, 0, 1000);
// destructor releases key 0
```

補助関数:

```cpp
ComPtr<IDXGIKeyedMutex> GetKeyedMutex(ID3D11Resource* resource);
bool HasKeyedMutex(ID3D11Resource* resource) noexcept;
```

`D3D11ScopedKeyedMutexAcquire` は acquire/release を RAII 化します。destructor は `noexcept` で、release 失敗を外へ投げません。明示的に失敗を扱いたい場合は `ReleaseNow()` を使います。

keyed mutex は key protocol を隠しません。producer / consumer 間で `AcquireSync(key)` と `ReleaseSync(nextKey)` の順序を設計する必要があります。

---

## D3D11FenceSupport

D3D11 fence は D3D11.4 / Windows 10 以降の機能に依存します。v1.8.0 以降では、使用前に support check ができます。

```cpp
D3D11FenceSupportInfo info = CheckD3D11FenceSupport(
    core->GetDevice(),
    core->GetImmediateContext());

if (!info.supported) {
    std::cout << "Fence unsupported: " << info.reason << "\n";
}
```

主な API:

```cpp
ComPtr<ID3D11Device5> TryGetD3D11Device5(ID3D11Device* device) noexcept;
ComPtr<ID3D11DeviceContext4> TryGetD3D11DeviceContext4(ID3D11DeviceContext* context) noexcept;

D3D11FenceSupportInfo CheckD3D11FenceSupport(
    ID3D11Device* device,
    ID3D11DeviceContext* context = nullptr) noexcept;

bool IsD3D11FenceSupported(
    ID3D11Device* device,
    ID3D11DeviceContext* context = nullptr) noexcept;
```

`D3D11FenceSupportInfo::supported` は `ID3D11Device5` と、context を渡した場合は `ID3D11DeviceContext4` が両方取得できると true になります。

---

## D3D11Fence

`D3D11Fence` は D3D11.4 の `ID3D11Fence` wrapper です。主に D3D12 との interop や、複数 device / API 間の timeline 同期に使います。

```cpp
D3D11Fence fence;
fence.Initialize(core->GetDevice());

fence.Signal(core->GetImmediateContext(), 1);
core->Flush();
fence.CpuWait(1);

D3D11SharedHandle fenceHandle = fence.CreateSharedHandleOwned();

D3D11Fence opened;
opened.OpenSharedHandle(core->GetDevice(), fenceHandle);
```

主な API:

```cpp
void Initialize(ID3D11Device5* device5, D3D11_FENCE_FLAG flags = D3D11_FENCE_FLAG_SHARED);
void Initialize(ID3D11Device* device, D3D11_FENCE_FLAG flags = D3D11_FENCE_FLAG_SHARED);

void OpenSharedHandle(ID3D11Device5* device5, HANDLE sharedHandle);
void OpenSharedHandle(ID3D11Device* device, HANDLE sharedHandle);
void OpenSharedHandle(ID3D11Device5* device5, const D3D11SharedHandle& sharedHandle);
void OpenSharedHandle(ID3D11Device* device, const D3D11SharedHandle& sharedHandle);

void Signal(ID3D11DeviceContext4* ctx, UINT64 value);
void Signal(ID3D11DeviceContext* ctx, UINT64 value);

void GpuWait(ID3D11DeviceContext4* ctx, UINT64 value);
void GpuWait(ID3D11DeviceContext* ctx, UINT64 value);

void CpuWait(UINT64 value);
HANDLE CreateSharedHandle() const;
D3D11SharedHandle CreateSharedHandleOwned() const;
UINT64 GetCompletedValue() const;
bool IsInitialized() const noexcept;
```

### GpuWait の注意

同じ immediate context 上で `GpuWait(N)` の後に、その値を満たす `Signal(N)` を積むと自己デッドロックします。

```text
悪い例:
context.Wait(fence, 1);
context.Signal(fence, 1); // Wait が先に実行されるため到達できない
```

`GpuWait` は、別 device / 別 API / 別 context が将来 signal する値を待つ用途で使ってください。単一 D3D11 immediate context 内の完了待ちは、`Signal + Flush + CpuWait` を基本にします。

---

## サンプル

```text
sample/24_Interop
```

このサンプルは、shared keyed-mutex texture の作成/open、key handoff、fence support 表示、D3D11.4 fence が使える環境での shared fence handle / signal / CpuWait を確認します。

---

## 互換パス

旧パスも v1.x では維持されます。

```cpp
#include <D3D11Helper/D3D11Core/D3D11Fence.hpp>
#include <D3D11Helper/D3D11Core/D3D11SharedResource.hpp>
```

新規コードでは以下を推奨します。

```cpp
#include <D3D11Helper/D3D11Interop/D3D11Interop.hpp>
```
