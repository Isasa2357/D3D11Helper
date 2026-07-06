# D3D11GpuTimer

`D3D11GpuTimer` は、D3D11 timestamp query を使った同期 GPU timer です。

v1.5.0 では、単発区間の GPU 実行時間を測るための最小 API として追加しています。

---

## Include

```cpp
#include <D3D11Helper/D3D11Diagnostics/D3D11GpuTimer.hpp>
```

または:

```cpp
#include <D3D11Helper/D3D11Diagnostics/D3D11Diagnostics.hpp>
```

---

## API

```cpp
struct D3D11GpuTimerResult {
    bool completed = false;
    bool valid = false;
    bool disjoint = false;

    UINT64 startTimestamp = 0;
    UINT64 endTimestamp = 0;
    UINT64 frequency = 0;

    double seconds = 0.0;
    double milliseconds = 0.0;
};

class D3D11GpuTimer {
public:
    void Initialize(ID3D11Device* device);
    void Reset() noexcept;

    bool IsInitialized() const noexcept;
    bool IsRunning() const noexcept;
    bool HasEnded() const noexcept;

    void Begin(ID3D11DeviceContext* context);
    void End(ID3D11DeviceContext* context);

    D3D11GpuTimerResult Resolve(ID3D11DeviceContext* context, bool wait = true);
};

class D3D11ScopedGpuTimer {
public:
    D3D11ScopedGpuTimer(D3D11GpuTimer& timer, ID3D11DeviceContext* context);
    ~D3D11ScopedGpuTimer() noexcept;
};
```

---

## 使い方

```cpp
D3D11GpuTimer timer;
timer.Initialize(core->GetDevice());

timer.Begin(core->GetImmediateContext());
// GPU work here
timer.End(core->GetImmediateContext());

core->Flush();
const D3D11GpuTimerResult result = timer.Resolve(core->GetImmediateContext(), true);

if (result.completed && result.valid) {
    std::cout << result.milliseconds << " ms\n";
}
```

---

## Result fields

| Field | 意味 |
| --- | --- |
| `completed` | query result を取得できた |
| `valid` | disjoint ではなく、frequency も有効で、時間を計算できた |
| `disjoint` | GPU timestamp が disjoint だった |
| `frequency` | timestamp frequency |
| `seconds` | GPU elapsed seconds |
| `milliseconds` | GPU elapsed milliseconds |

`completed == true` でも `valid == false` の場合があります。たとえば timestamp disjoint が発生した場合です。

---

## wait parameter

```cpp
Resolve(context, true);
```

`wait = true` の場合、query result が返るまで待ちます。

```cpp
Resolve(context, false);
```

`wait = false` の場合、まだ query result が返らないと `completed == false` の result を返します。

---

## 制限

v1.5.0 の `D3D11GpuTimer` は以下に限定しています。

```text
- immediate context 前提
- single interval timer
- synchronous resolve
- timestamp/disjoint query のみ
```

非対応:

```text
- async multi-frame timer ring
- nested timer manager
- file export
- CPU/GPU correlation timeline
- PIX marker integration
```
