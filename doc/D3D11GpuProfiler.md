# D3D11GpuProfiler

`D3D11GpuProfiler` は、`D3D11GpuTimer` を複数の named sample として扱う simple single-frame profiler です。

v1.5.0 では、複雑な profiler framework ではなく、1 frame 内で名前付き GPU 区間を測るための薄い helper として追加しています。

---

## Include

```cpp
#include <D3D11Helper/D3D11Diagnostics/D3D11GpuProfiler.hpp>
```

または:

```cpp
#include <D3D11Helper/D3D11Diagnostics/D3D11Diagnostics.hpp>
```

---

## API

```cpp
struct D3D11GpuProfilerSampleResult {
    std::string name;
    bool completed = false;
    bool valid = false;
    bool disjoint = false;
    double milliseconds = 0.0;
};

class D3D11GpuProfiler {
public:
    void Initialize(ID3D11Device* device);
    void Reset();

    bool IsInitialized() const noexcept;

    void BeginFrame(ID3D11DeviceContext* context);
    void EndFrame(ID3D11DeviceContext* context);

    UINT BeginSample(ID3D11DeviceContext* context, std::string name);
    void EndSample(ID3D11DeviceContext* context, UINT sampleId);

    std::vector<D3D11GpuProfilerSampleResult> Resolve(
        ID3D11DeviceContext* context,
        bool wait = true);
};

class D3D11ScopedGpuProfileSample {
public:
    D3D11ScopedGpuProfileSample(
        D3D11GpuProfiler& profiler,
        ID3D11DeviceContext* context,
        std::string name);

    ~D3D11ScopedGpuProfileSample() noexcept;
};
```

---

## 使い方

```cpp
D3D11GpuProfiler profiler;
profiler.Initialize(core->GetDevice());

ID3D11DeviceContext* ctx = core->GetImmediateContext();

profiler.BeginFrame(ctx);

const UINT upload = profiler.BeginSample(ctx, "upload");
// GPU work...
profiler.EndSample(ctx, upload);

{
    D3D11ScopedGpuProfileSample scoped(profiler, ctx, "dispatch");
    // GPU work...
}

profiler.EndFrame(ctx);
core->Flush();

const auto results = profiler.Resolve(ctx, true);
for (const auto& sample : results) {
    if (sample.completed && sample.valid) {
        std::cout << sample.name << ": " << sample.milliseconds << " ms\n";
    }
}
```

---

## Frame model

v1.5.0 の profiler は single-frame モデルです。

```text
BeginFrame
  BeginSample("A")
  EndSample(A)
  BeginSample("B")
  EndSample(B)
EndFrame
Resolve
```

`BeginFrame()` は内部 sample list を clear します。`Resolve()` は `EndFrame()` 後に呼びます。

---

## Scoped sample

`D3D11ScopedGpuProfileSample` を使うと、scope 終了時に `EndSample()` が呼ばれます。

```cpp
{
    D3D11ScopedGpuProfileSample sample(profiler, ctx, "postprocess");
    // GPU work...
}
```

デストラクタは `noexcept` です。終了時に例外が発生した場合は握りつぶします。終了を厳密に扱いたい場合は、明示的に `BeginSample()` / `EndSample()` を使ってください。

---

## 制限

v1.5.0 では以下に限定しています。

```text
- immediate context 前提
- 1 frame 内の named samples
- Resolve は同期または単発 non-wait
- nested sample は禁止ではないが、階層 profiler としては扱わない
```

非対応:

```text
- async multi-frame profiler ring
- Chrome trace export
- CSV / JSON output
- UI overlay
- GPU/CPU timeline correlation
- frame pacing analysis
```
