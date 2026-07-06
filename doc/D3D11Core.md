# D3D11Core

`D3D11Core` は、D3D11 の device / immediate context / deferred context / DXGI を束ねる実行基盤です。

## Include

```cpp
#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
```

## 主な型

### D3D11CoreConfig

```cpp
struct D3D11CoreConfig {
    bool enableDebugLayer;
    bool enableInfoQueue;
    bool preferHighPerformanceAdapter;
    bool allowWarpAdapter;
    D3D_FEATURE_LEVEL minimumFeatureLevel;
    bool enableMultithreadProtection;
    bool breakOnError;
    bool breakOnCorruption;
    bool breakOnWarning;
};
```

### D3D11Core

```cpp
void Initialize(const D3D11CoreConfig& config = {});
void InitializeWithAdapterLuid(LUID luid, const D3D11CoreConfig& config = {});
static std::shared_ptr<D3D11Core> CreateShared(const D3D11CoreConfig& config = {});
static std::shared_ptr<D3D11Core> CreateSharedWithAdapterLuid(LUID, const D3D11CoreConfig& = {});
```

よく使うショートカット:

- `GetDevice()` / `GetDevice5()`
- `GetImmediateContext()` / `GetImmediateContext4()`
- `GetFactory()`
- `GetAdapterLuid()`
- `IsSameAdapter(LUID)`
- `GetFeatureLevel()`
- `IsMultithreadProtected()`
- `CreateDeferredContext()`
- `ExecuteCommandList(commandList, restoreContextState)`
- `Flush()`

### D3D11DeviceContext

Factory / Adapter / Device / ImmediateContext を保持します。D3D11.4 の `ID3D11Device5` / `ID3D11DeviceContext4` が取得できる場合は保持します。

### D3D11DeferredContext

ワーカースレッドで D3D11 command list を記録するための薄い wrapper です。

```cpp
ID3D11DeviceContext* Get() const;
ComPtr<ID3D11CommandList> FinishCommandList(bool restoreDeferredContextState = false);
```

## v1.1.0 で Core から分離したもの

次の機能は Core から別モジュールへ整理されました。

| 旧位置 | 新位置 |
|---|---|
| `D3D11Common.hpp` | `D3D11Foundation/D3D11Common.hpp` |
| `ThrowIfFailed.hpp` | `D3D11Foundation/ThrowIfFailed.hpp` |
| `DxgiUtil.hpp` | `D3D11Foundation/DxgiUtil.hpp` |
| `DxgiAdapterSelector.hpp` | `D3D11Foundation/DxgiAdapterSelector.hpp` |
| `D3D11FormatUtil.hpp` | `D3D11Foundation/D3D11FormatUtil.hpp` |
| `D3D11Debug.hpp` | `D3D11Diagnostics/D3D11Debug.hpp` |
| `D3D11Fence.hpp` | `D3D11Interop/D3D11Fence.hpp` |
| `D3D11SharedResource.hpp` | `D3D11Interop/D3D11SharedResource.hpp` |

旧 include path は v1.x 互換 wrapper として残します。
