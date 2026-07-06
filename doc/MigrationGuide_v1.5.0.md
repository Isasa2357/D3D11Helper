# D3D11Helper v1.5.0 Migration Guide

## 概要

`v1.5.0` は backward-compatible な機能追加です。既存コードの必須変更はありません。

新規 diagnostics API を使う場合は、`D3D11Diagnostics` module から include します。

```cpp
#include <D3D11Helper/D3D11Diagnostics/D3D11Diagnostics.hpp>
```

---

## 1. Device lost check の追加

従来、device lost 判定は各アプリ側で `GetDeviceRemovedReason()` を直接読む必要がありました。

v1.5.0 では次の helper を使えます。

```cpp
const auto info = CheckDeviceLost(core->GetDevice());
if (info.deviceLost) {
    std::cerr << "Device lost: " << info.name << "\n";
}
```

例外にしたい場合:

```cpp
ThrowIfDeviceLost(core->GetDevice());
```

---

## 2. InfoQueue の扱い

Debug Layer が有効な場合だけ InfoQueue が使えます。

安全に試す場合:

```cpp
auto queue = TryGetD3D11InfoQueue(core->GetDevice());
if (queue) {
    ClearInfoQueueMessages(queue.Get());
}
```

必須として扱う場合:

```cpp
auto queue = GetD3D11InfoQueue(core->GetDevice());
```

InfoQueue が無い環境では `GetD3D11InfoQueue()` は例外を投げます。

---

## 3. GPU timer の導入

単発の GPU 区間を測るには `D3D11GpuTimer` を使います。

```cpp
D3D11GpuTimer timer;
timer.Initialize(core->GetDevice());

timer.Begin(ctx);
// GPU work
timer.End(ctx);

core->Flush();
const auto result = timer.Resolve(ctx, true);
```

`result.valid == true` の場合に `milliseconds` を参照します。

---

## 4. GPU profiler の導入

1 frame 内で複数の named sample を測るには `D3D11GpuProfiler` を使います。

```cpp
D3D11GpuProfiler profiler;
profiler.Initialize(core->GetDevice());

profiler.BeginFrame(ctx);
const UINT id = profiler.BeginSample(ctx, "blur");
// GPU work
profiler.EndSample(ctx, id);
profiler.EndFrame(ctx);

core->Flush();
const auto samples = profiler.Resolve(ctx, true);
```

RAII helper もあります。

```cpp
{
    D3D11ScopedGpuProfileSample sample(profiler, ctx, "postprocess");
    // GPU work
}
```

---

## 5. 既存 D3D11Debug API との関係

v1.5.0 は既存の `D3D11Debug` API を置き換えません。

```cpp
D3D11Debug::SetupInfoQueue(...);
D3D11Debug::ReportLiveObjects(...);
D3D11Debug::SetDebugName(...);
```

これらは従来通り使えます。

新規 InfoQueue helper は、より細かい retrieval / clear / severity filter を扱う補助 API です。

---

## 6. 注意点

GPU timer / profiler は D3D11 query を使います。

```text
- very short GPU work は 0 ms 付近になることがある
- disjoint が発生した場合は valid=false になる
- wait=true の Resolve は GPU 完了を待つ
- wait=false の Resolve は completed=false を返すことがある
```

高性能な非同期 profiler が必要な場合は、後続バージョンで multi-frame ring として設計します。
