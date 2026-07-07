# DesignSpec v1.8.0 - D3D11Interop

## 目的

`v1.8.0` では `D3D11Interop` を、既存の `D3D11SharedResource` / `D3D11Fence` から一段使いやすい interop layer へ拡張する。

対象は DirectX / DXGI に閉じた相互運用 primitive のみである。

- NT shared handle ownership
- shared Texture2D creation / open helper
- keyed mutex helper
- D3D11.4 fence helper hardening
- interop runtime tests
- console-only sample

CUDA / Varjo / NVENC / Media Foundation / D3D12Helper への直接依存は入れない。

## 現在の基準状態

実装開始前の基準は `v1.7.0` である。

確認項目:

- `CMakeLists.txt` の `project(D3D11Helper VERSION 1.7.0)` が正しい。
- `D3D11Gpu` には v1.7.0 の `D3D11View` / `D3D11State` が登録済みである。
- `D3D11Interop.hpp` は現状 `D3D11SharedResource.hpp` と `D3D11Fence.hpp` の umbrella である。
- `D3D11SharedResource` は NT shared handle の raw `HANDLE` 作成 / open を持つ。
- `D3D11Fence` は D3D11.4 fence wrapper を持つ。

v1.8.0 では、この既存 API を壊さずに補助 API を追加する。

## 既存コードで先に直すべき点

`D3D11Helpers.hpp` の `CreateSharedTexture2D` 宣言は `D3D11SharedTextureSyncMode syncMode` を持っているが、`src/D3D11Helpers.cpp` 側の定義が古い signature のままになっている可能性がある。

v1.8.0 Round 1 で必ず修正する。

期待する修正方針:

- 既存 API の名前は維持する。
- `D3D11SharedTextureSyncMode::SharedFence` では
  `D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED`
  を使う。
- `D3D11SharedTextureSyncMode::KeyedMutex` では
  `D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX`
  を使う。
- 既存 `CreateSharedTexture2D(D3D11Core&, ...)` は新しい Interop helper に委譲してよい。

## 実装範囲

### Round 1: Shared handle / shared texture

追加予定:

```text
include/D3D11Helper/D3D11Interop/D3D11SharedHandle.hpp
include/D3D11Helper/D3D11Interop/D3D11SharedTexture.hpp

src/D3D11SharedHandle.cpp
src/D3D11SharedTexture.cpp

Test/test_interop_shared.cpp
```

更新予定:

```text
CMakeLists.txt
include/D3D11Helper/D3D11Interop/D3D11Interop.hpp
include/D3D11Helper/D3D11Interop/D3D11SharedResource.hpp
include/D3D11Helper/D3D11Gpu/D3D11Helpers.hpp
src/D3D11SharedResource.cpp
src/D3D11Helpers.cpp
Test/CMakeLists.txt
Test/test_module_headers.cpp
```

### Round 2: Keyed mutex

追加予定:

```text
include/D3D11Helper/D3D11Interop/D3D11KeyedMutex.hpp
src/D3D11KeyedMutex.cpp
Test/test_interop_keyed_mutex.cpp
```

更新予定:

```text
CMakeLists.txt
include/D3D11Helper/D3D11Interop/D3D11Interop.hpp
Test/CMakeLists.txt
Test/test_module_headers.cpp
```

### Round 3: Fence hardening / sample

追加予定:

```text
include/D3D11Helper/D3D11Interop/D3D11FenceSupport.hpp
src/D3D11FenceSupport.cpp
Test/test_interop_fence.cpp
sample/24_Interop/main.cpp
```

更新予定:

```text
CMakeLists.txt
include/D3D11Helper/D3D11Interop/D3D11Interop.hpp
include/D3D11Helper/D3D11Interop/D3D11Fence.hpp
src/D3D11Fence.cpp
Test/CMakeLists.txt
Test/test_module_headers.cpp
sample/CMakeLists.txt
```

## Public API: D3D11SharedHandle

```cpp
class D3D11SharedHandle {
public:
    D3D11SharedHandle() noexcept;
    explicit D3D11SharedHandle(HANDLE handle) noexcept;
    ~D3D11SharedHandle();

    D3D11SharedHandle(const D3D11SharedHandle&) = delete;
    D3D11SharedHandle& operator=(const D3D11SharedHandle&) = delete;

    D3D11SharedHandle(D3D11SharedHandle&& other) noexcept;
    D3D11SharedHandle& operator=(D3D11SharedHandle&& other) noexcept;

    bool IsValid() const noexcept;
    explicit operator bool() const noexcept;

    HANDLE Get() const noexcept;
    HANDLE Release() noexcept;
    void Reset(HANDLE handle = nullptr) noexcept;

    D3D11SharedHandle Duplicate(
        DWORD desiredAccess = 0,
        BOOL inheritHandle = FALSE) const;
};
```

### 方針

- `D3D11SharedHandle` は `HANDLE` を所有し、destructor で `CloseHandle` する。
- `INVALID_HANDLE_VALUE` は無効値として扱い、`nullptr` と同様に close しない。
- move-only。
- `Release()` は所有権を呼び出し側へ渡す。
- `Duplicate()` は `DuplicateHandle` を使い、新しい owned handle を返す。
- `desiredAccess == 0` の場合は `DUPLICATE_SAME_ACCESS` を使う。

### 互換

既存 `D3D11SharedResource::CreateSharedHandle()` は raw `HANDLE` を返す互換 API として残す。
新規 API として owned handle 版を追加する。

```cpp
static D3D11SharedHandle CreateSharedHandleOwned(
    ID3D11Resource* resource,
    LPCWSTR name = nullptr);
```

## Public API: D3D11SharedTexture

```cpp
enum class D3D11SharedTextureSyncMode {
    SharedFence,
    KeyedMutex
};

struct D3D11SharedTexture2DDesc {
    UINT width = 0;
    UINT height = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

    UINT bindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    D3D11_USAGE usage = D3D11_USAGE_DEFAULT;
    UINT cpuAccessFlags = 0;

    UINT mipLevels = 1;
    UINT arraySize = 1;
    UINT sampleCount = 1;
    UINT sampleQuality = 0;

    UINT miscFlags = 0;
    D3D11SharedTextureSyncMode syncMode = D3D11SharedTextureSyncMode::SharedFence;
};

struct D3D11SharedTexture2D {
    ComPtr<ID3D11Texture2D> texture;
    D3D11SharedHandle handle;

    ID3D11Texture2D* Get() const noexcept;
    HANDLE GetHandle() const noexcept;
    explicit operator bool() const noexcept;
};
```

```cpp
UINT MakeSharedTextureMiscFlags(
    D3D11SharedTextureSyncMode syncMode,
    UINT extraMiscFlags = 0);

void ValidateSharedTexture2DDesc(
    const D3D11SharedTexture2DDesc& desc,
    const char* functionName);

ComPtr<ID3D11Texture2D> CreateSharedTexture2D(
    ID3D11Device* device,
    const D3D11SharedTexture2DDesc& desc);

D3D11SharedTexture2D CreateSharedTexture2DWithHandle(
    ID3D11Device* device,
    const D3D11SharedTexture2DDesc& desc,
    LPCWSTR name = nullptr);

ComPtr<ID3D11Texture2D> OpenSharedTexture2D(
    ID3D11Device* device,
    HANDLE handle);

ComPtr<ID3D11Texture2D> OpenSharedTexture2D(
    ID3D11Device* device,
    const D3D11SharedHandle& handle);
```

### Validation

`ValidateSharedTexture2DDesc` は最低限以下を検証する。

- `width > 0`
- `height > 0`
- `format != DXGI_FORMAT_UNKNOWN`
- `mipLevels > 0`
- `arraySize > 0`
- `sampleCount > 0`
- `usage == D3D11_USAGE_DEFAULT`
- `cpuAccessFlags == 0`
- `bindFlags != 0`
- `syncMode` が既知値
- `miscFlags` に `D3D11_RESOURCE_MISC_SHARED` / `D3D11_RESOURCE_MISC_SHARED_NTHANDLE` / `D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX` を直接混ぜても破綻しないが、基本は `MakeSharedTextureMiscFlags` が付与する。

### Misc flags

```cpp
SharedFence:
    D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
    D3D11_RESOURCE_MISC_SHARED

KeyedMutex:
    D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
    D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX
```

`SHARED` と `SHARED_KEYEDMUTEX` は同時に指定しない。

## Public API: D3D11KeyedMutex

```cpp
class D3D11KeyedMutex {
public:
    D3D11KeyedMutex() = default;

    explicit D3D11KeyedMutex(ID3D11Resource* resource);
    void Attach(ID3D11Resource* resource);
    void Reset() noexcept;

    bool IsAttached() const noexcept;
    IDXGIKeyedMutex* Get() const noexcept;

    bool Acquire(UINT64 key, DWORD timeoutMs);
    void AcquireOrThrow(UINT64 key, DWORD timeoutMs);
    void Release(UINT64 key);

private:
    ComPtr<IDXGIKeyedMutex> m_mutex;
};
```

```cpp
class D3D11ScopedKeyedMutexAcquire {
public:
    D3D11ScopedKeyedMutexAcquire(
        D3D11KeyedMutex& mutex,
        UINT64 acquireKey,
        UINT64 releaseKey,
        DWORD timeoutMs);

    ~D3D11ScopedKeyedMutexAcquire() noexcept;

    D3D11ScopedKeyedMutexAcquire(const D3D11ScopedKeyedMutexAcquire&) = delete;
    D3D11ScopedKeyedMutexAcquire& operator=(const D3D11ScopedKeyedMutexAcquire&) = delete;

    bool Acquired() const noexcept;
    void ReleaseNow();

private:
    D3D11KeyedMutex* m_mutex = nullptr;
    UINT64 m_releaseKey = 0;
    bool m_acquired = false;
};
```

### Semantics

- `Attach()` は `resource->QueryInterface(IDXGIKeyedMutex)` を行う。
- keyed mutex 付きで作られていない resource の場合は例外。
- `Acquire()` は `AcquireSync` を呼ぶ。
- `DXGI_ERROR_WAIT_TIMEOUT` の場合だけ `false` を返す。
- それ以外の失敗 HRESULT は例外。
- `AcquireOrThrow()` は timeout も例外。
- `Release()` は `ReleaseSync` を呼ぶ。
- scoped acquire は constructor で acquire し、destructor で release する。
- destructor は `noexcept`。release 失敗時は握りつぶす。
- `ReleaseNow()` は明示 release し、二重 release を防ぐ。

## Public API: D3D11FenceSupport

```cpp
ComPtr<ID3D11Device5> TryGetD3D11Device5(ID3D11Device* device);
ComPtr<ID3D11Device5> GetD3D11Device5(ID3D11Device* device);

ComPtr<ID3D11DeviceContext4> TryGetD3D11DeviceContext4(ID3D11DeviceContext* context);
ComPtr<ID3D11DeviceContext4> GetD3D11DeviceContext4(ID3D11DeviceContext* context);

bool IsD3D11FenceSupported(ID3D11Device* device) noexcept;
```

## D3D11Fence hardening

既存 `D3D11Fence` は API を壊さず、以下を追加・修正する。

```cpp
bool IsInitialized() const noexcept;

bool CpuWait(UINT64 value, DWORD timeoutMs);
D3D11SharedHandle CreateSharedHandleOwned() const;
```

既存:

```cpp
void CpuWait(UINT64 value);
HANDLE CreateSharedHandle() const;
```

は残す。

### 修正方針

- `Initialize()` / `OpenSharedHandle()` の冒頭で既存状態を `Destroy()` してから新規作成する。
- `CpuWait(UINT64)` は従来どおり無限待ち。
- `CpuWait(UINT64, DWORD timeoutMs)` は timeout 時に `false` を返す。
- `WaitForSingleObject` が `WAIT_OBJECT_0` 以外で timeout でもない場合は例外。
- `CreateSharedHandleOwned()` は `D3D11SharedHandle` を返す。
- raw `HANDLE CreateSharedHandle()` は互換のため残す。

## Tests

### Test/test_interop_shared.cpp

内容:

- `D3D11SharedHandle` default / move / reset / release
- `D3D11SharedHandle::Duplicate`
- `CreateSharedTexture2D`
- `CreateSharedTexture2DWithHandle`
- `OpenSharedTexture2D`
- opened texture desc が一致すること
- invalid desc が throw すること
- 既存 `CreateSharedTexture2D(D3D11Core&, ...)` が link / run できること
- `syncMode == SharedFence` の misc flag
- `syncMode == KeyedMutex` の misc flag

注意:

- named shared handle はテストしなくてよい。
- external process / D3D12 device は不要。
- same device で open できることを確認する。

### Test/test_interop_keyed_mutex.cpp

内容:

- keyed mutex shared texture を作る
- `D3D11KeyedMutex::Attach`
- `Acquire(0, timeout)` -> true
- `Release(1)`
- `Acquire(1, timeout)` -> true
- `Release(0)`
- scoped acquire / release
- keyed mutex なし texture に attach すると throw
- timeout path は deterministic に作りにくいので、必須 test にしない。

### Test/test_interop_fence.cpp

内容:

- `TryGetD3D11Device5`
- `TryGetD3D11DeviceContext4`
- unsupported environment では skip 的に成功終了
- supported environment では:
  - `D3D11Fence::Initialize`
  - `IsInitialized`
  - `CreateSharedHandle`
  - `CreateSharedHandleOwned`
  - `OpenSharedHandle`
  - `Signal(1)`
  - `Flush`
  - `CpuWait(1, timeoutMs)`
  - `GetCompletedValue() >= 1`

禁止:

- same immediate context で `GpuWait` 後に `Signal` を積む self-deadlock pattern。
- WARP で不安定な GPU wait test。

## Sample

### sample/24_Interop

console-only sample。

内容:

1. `D3D11Core` 作成
2. shared Texture2D を作成
3. shared handle を作成
4. same device で open
5. keyed mutex shared texture を作成し、Acquire/Release
6. fence support があれば fence を作成し、Signal/CpuWait
7. 結果を `std::cout` に出す

禁止:

- window
- file I/O
- D3D12 dependency
- external process
- CUDA / Varjo / NVENC / Media Foundation

## CMake / umbrella registration

追加 header/source は必ず root `CMakeLists.txt` に登録する。

追加予定:

```text
src/D3D11SharedHandle.cpp
src/D3D11SharedTexture.cpp
src/D3D11KeyedMutex.cpp
src/D3D11FenceSupport.cpp

include/D3D11Helper/D3D11Interop/D3D11SharedHandle.hpp
include/D3D11Helper/D3D11Interop/D3D11SharedTexture.hpp
include/D3D11Helper/D3D11Interop/D3D11KeyedMutex.hpp
include/D3D11Helper/D3D11Interop/D3D11FenceSupport.hpp
```

`D3D11Interop.hpp` には以下を include する。

```cpp
#include <D3D11Helper/D3D11Interop/D3D11SharedResource.hpp>
#include <D3D11Helper/D3D11Interop/D3D11SharedHandle.hpp>
#include <D3D11Helper/D3D11Interop/D3D11SharedTexture.hpp>
#include <D3D11Helper/D3D11Interop/D3D11KeyedMutex.hpp>
#include <D3D11Helper/D3D11Interop/D3D11Fence.hpp>
#include <D3D11Helper/D3D11Interop/D3D11FenceSupport.hpp>
```

## Build/test audit

各 round の build script には最低限以下を含める。

```bat
findstr /N "D3D11SharedHandle D3D11SharedTexture D3D11KeyedMutex D3D11FenceSupport" CMakeLists.txt
findstr /N "D3D11SharedHandle D3D11SharedTexture D3D11KeyedMutex D3D11FenceSupport" include\D3D11Helper\D3D11Interop\D3D11Interop.hpp

dir include\D3D11Helper\D3D11Interop\D3D11SharedHandle.hpp
dir include\D3D11Helper\D3D11Interop\D3D11SharedTexture.hpp
dir include\D3D11Helper\D3D11Interop\D3D11KeyedMutex.hpp
dir include\D3D11Helper\D3D11Interop\D3D11FenceSupport.hpp

dir src\D3D11SharedHandle.cpp
dir src\D3D11SharedTexture.cpp
dir src\D3D11KeyedMutex.cpp
dir src\D3D11FenceSupport.cpp
```

Round 1 ではまだ存在しない files は audit 対象から外してよい。
ただし最終 implementation round ではすべて確認する。

## Out of scope

v1.8.0 では以下を実装しない。

- D3D12 device / D3D12 resource wrapper
- D3D11On12
- CUDA / Varjo / NVENC / Media Foundation integration
- external process sample
- security descriptor customization beyond default nullptr
- cross-process named handle registry
- GPU wait を使った複雑な scheduling
- resource lifetime graph
- automatic hazard tracking
- keyed mutex timeout recovery policy
- NT handle duplication to another process

## Completion criteria before docs

- `D3D11SharedHandle` がある。
- `D3D11SharedTexture` がある。
- `D3D11KeyedMutex` がある。
- `D3D11FenceSupport` がある。
- 既存 `D3D11SharedResource` / `D3D11Fence` が壊れていない。
- 既存 `CreateSharedTexture2D(D3D11Core&, ...)` の signature mismatch が修正されている。
- `D3D11Interop.hpp` から新規 headers が include されている。
- `Test/test_interop_shared.cpp` が通る。
- `Test/test_interop_keyed_mutex.cpp` が通る。
- `Test/test_interop_fence.cpp` が unsupported 環境で安全に skip でき、supported 環境で通る。
- `sample/24_Interop` が build できる。
- `build_and_test.cmd` が通る。
- `CMakeLists.txt` の version はまだ `1.7.0` のまま。
