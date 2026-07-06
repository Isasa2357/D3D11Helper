# D3D11Diagnostics

`D3D11Diagnostics` は、D3D11 Debug Layer / InfoQueue / LiveObject report / device lost / GPU timer / GPU profiler などの診断機能を扱うモジュールです。

## Include

```cpp
#include <D3D11Helper/D3D11Diagnostics/D3D11Diagnostics.hpp>
```

個別 include:

```cpp
#include <D3D11Helper/D3D11Diagnostics/D3D11Debug.hpp>
#include <D3D11Helper/D3D11Diagnostics/D3D11DeviceLost.hpp>
#include <D3D11Helper/D3D11Diagnostics/D3D11InfoQueue.hpp>
#include <D3D11Helper/D3D11Diagnostics/D3D11GpuTimer.hpp>
#include <D3D11Helper/D3D11Diagnostics/D3D11GpuProfiler.hpp>
```

---

## D3D11Debug

既存の debug-layer / InfoQueue / live object helper です。

```cpp
namespace D3D11Debug {
void SetupInfoQueue(
    ID3D11Device* device,
    bool breakOnError,
    bool breakOnCorruption,
    bool breakOnWarning);

void ReportLiveObjects(ID3D11Device* device);

template <typename T>
void SetDebugName(T* object, const char* name);
}
```

---

## Device lost helpers

v1.5.0 では、device removed / reset / hung などを扱う helper を追加しています。

```cpp
struct D3D11DeviceLostInfo {
    HRESULT reason = S_OK;
    bool deviceLost = false;
    const char* name = "S_OK";
};

bool IsDeviceLost(HRESULT hr) noexcept;
const char* DeviceLostReasonName(HRESULT hr) noexcept;
D3D11DeviceLostInfo CheckDeviceLost(ID3D11Device* device);
void ThrowIfDeviceLost(ID3D11Device* device);
```

使用例:

```cpp
const auto info = CheckDeviceLost(core->GetDevice());
if (info.deviceLost) {
    std::cerr << "Device lost: " << info.name << "\n";
}
```

`CheckDeviceLost()` は `ID3D11Device::GetDeviceRemovedReason()` を読みます。通常状態では `S_OK` が返ります。

---

## InfoQueue helpers

Debug Layer が有効な環境では `ID3D11InfoQueue` を取得できます。v1.5.0 では、取得・message count・clear・break 設定・severity filter を補助する API を追加しています。

```cpp
ComPtr<ID3D11InfoQueue> TryGetD3D11InfoQueue(ID3D11Device* device);
ComPtr<ID3D11InfoQueue> GetD3D11InfoQueue(ID3D11Device* device);

UINT64 GetInfoQueueMessageCount(ID3D11InfoQueue* queue);
void ClearInfoQueueMessages(ID3D11InfoQueue* queue);

void SetInfoQueueBreakOnSeverity(
    ID3D11InfoQueue* queue,
    D3D11_MESSAGE_SEVERITY severity,
    bool enable);

void AddInfoQueueStorageFilterDenySeverity(
    ID3D11InfoQueue* queue,
    const D3D11_MESSAGE_SEVERITY* severities,
    UINT severityCount);
```

`TryGetD3D11InfoQueue()` は InfoQueue が使えない場合に `nullptr` を返します。`GetD3D11InfoQueue()` は使えない場合に例外を投げます。

---

## GPU timer

`D3D11GpuTimer` は `D3D11_QUERY_TIMESTAMP_DISJOINT` と `D3D11_QUERY_TIMESTAMP` を使う同期 GPU timer です。

```cpp
D3D11GpuTimer timer;
timer.Initialize(core->GetDevice());

timer.Begin(core->GetImmediateContext());
// GPU work...
timer.End(core->GetImmediateContext());
core->Flush();

const auto result = timer.Resolve(core->GetImmediateContext(), true);
```

詳しくは [`D3D11GpuTimer.md`](D3D11GpuTimer.md) を参照してください。

---

## GPU profiler

`D3D11GpuProfiler` は `D3D11GpuTimer` を複数 sample として扱う simple single-frame profiler です。

```cpp
D3D11GpuProfiler profiler;
profiler.Initialize(core->GetDevice());

profiler.BeginFrame(ctx);
const UINT id = profiler.BeginSample(ctx, "work");
// GPU work...
profiler.EndSample(ctx, id);
profiler.EndFrame(ctx);

const auto results = profiler.Resolve(ctx, true);
```

詳しくは [`D3D11GpuProfiler.md`](D3D11GpuProfiler.md) を参照してください。

---

## v1.5.0 scope

対応:

- device lost reason classification
- InfoQueue optional retrieval / basic operations
- timestamp-query based GPU timer
- single-frame named sample profiler
- runtime tests
- console sample

非対応:

- async multi-frame profiler ring
- Chrome trace export
- CSV / JSON output
- UI overlay
- PIX / ETW integration
- vendor-specific GPU counters
- automatic frame pacing analysis

---

## 互換パス

旧パスも v1.x では維持されます。

```cpp
#include <D3D11Helper/D3D11Core/D3D11Debug.hpp>
```
