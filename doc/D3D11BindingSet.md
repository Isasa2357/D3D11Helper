# D3D11BindingSet

`D3D11BindingSet` は、v1.4.0 で追加された **compute-stage resource binding helper** です。

D3D11 の compute stage では、SRV / UAV / constant buffer / sampler をそれぞれ `CSSet*` 系 API で slot に設定します。`D3D11ComputeBindingSet` は、その定型処理を小さくまとめるための非所有 helper です。

---

## Include

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11BindingSet.hpp>
```

または GPU umbrella:

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
```

---

## 基本方針

### 非所有 binding set

`D3D11ComputeBindingSet` は、登録された SRV / UAV / buffer / sampler を所有しません。

```text
D3D11ComputeBindingSet
  - raw ID3D11ShaderResourceView*
  - raw ID3D11UnorderedAccessView*
  - raw ID3D11Buffer*
  - raw ID3D11SamplerState*
```

そのため、binding set に渡した object の lifetime は呼び出し側が管理します。

```cpp
D3D11ComputeBindingSet bindings;
bindings.SetShaderResource(0, srv.Get());
bindings.SetUnorderedAccess(0, uav.Get());
bindings.SetConstantBuffer(0, constantBuffer.Get());
bindings.SetSampler(0, sampler.Get());

bindings.Bind(context);
```

### Scoped previous-state restore

`D3D11ScopedComputeBindings` は、指定された binding set が触る slot range の previous state を `CSGet*` で取得し、`ComPtr` で保持します。

```cpp
{
    D3D11ScopedComputeBindings scoped(context, bindings);
    // compute dispatch or helper processing
} // previous compute-stage bindings are restored here
```

`D3D11ScopedComputeBindings` が所有するのは previous state だけです。新しく bind する resource は所有しません。

---

## D3D11ComputeBindingSet

```cpp
class D3D11ComputeBindingSet {
public:
    static constexpr UINT ShaderResourceSlotCount = D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT;
    static constexpr UINT UnorderedAccessSlotCount = D3D11_1_UAV_SLOT_COUNT;
    static constexpr UINT ConstantBufferSlotCount = D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;
    static constexpr UINT SamplerSlotCount = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
    static constexpr UINT KeepUavInitialCount = UINT(-1);

    void Clear() noexcept;
    bool Empty() const noexcept;

    void SetShaderResource(UINT slot, ID3D11ShaderResourceView* srv);
    void SetUnorderedAccess(UINT slot, ID3D11UnorderedAccessView* uav, UINT initialCount = KeepUavInitialCount);
    void SetConstantBuffer(UINT slot, ID3D11Buffer* buffer);
    void SetSampler(UINT slot, ID3D11SamplerState* sampler);

    void Bind(ID3D11DeviceContext* context) const;
    void Unbind(ID3D11DeviceContext* context) const;
};
```

### Range tracking

`Set*` は、指定された slot を含む最小 contiguous range を内部で記録します。

例:

```cpp
D3D11ComputeBindingSet bindings;
bindings.SetShaderResource(2, srvA.Get());
bindings.SetShaderResource(5, srvB.Get());
```

この場合、SRV range は `2..5` になります。slot 3 と 4 は `nullptr` として bind されます。

これは D3D11 の `CSSetShaderResources(startSlot, count, array)` が contiguous range を要求するためです。

### Clear / Empty

```cpp
bindings.Clear();
bool empty = bindings.Empty();
```

`Clear()` は保持している raw pointer と range をすべて初期化します。resource object 自体は所有していないため解放しません。

---

## Bind / Unbind helper

自由関数として、stage-specific helper も提供します。

```cpp
void BindComputeShaderResources(ID3D11DeviceContext* context, UINT startSlot, UINT count, ID3D11ShaderResourceView* const* srvs);
void BindComputeUnorderedAccessViews(ID3D11DeviceContext* context, UINT startSlot, UINT count, ID3D11UnorderedAccessView* const* uavs, const UINT* initialCounts = nullptr);
void BindComputeConstantBuffers(ID3D11DeviceContext* context, UINT startSlot, UINT count, ID3D11Buffer* const* buffers);
void BindComputeSamplers(ID3D11DeviceContext* context, UINT startSlot, UINT count, ID3D11SamplerState* const* samplers);
```

Unbind helper:

```cpp
void UnbindComputeShaderResources(ID3D11DeviceContext* context, UINT startSlot, UINT count);
void UnbindComputeUnorderedAccessViews(ID3D11DeviceContext* context, UINT startSlot, UINT count);
void UnbindComputeConstantBuffers(ID3D11DeviceContext* context, UINT startSlot, UINT count);
void UnbindComputeSamplers(ID3D11DeviceContext* context, UINT startSlot, UINT count);
void D3D11UnbindComputeResources(ID3D11DeviceContext* context);
```

### Validation

これらの helper は、以下を不正入力として扱います。

- null `ID3D11DeviceContext*`
- slot range out of bounds
- `count != 0` なのに pointer array が null

`count == 0` は no-op です。

---

## D3D11ScopedComputeBindings

```cpp
class D3D11ScopedComputeBindings {
public:
    D3D11ScopedComputeBindings(ID3D11DeviceContext* context, const D3D11ComputeBindingSet& bindings);
    ~D3D11ScopedComputeBindings() noexcept;

    D3D11ScopedComputeBindings(const D3D11ScopedComputeBindings&) = delete;
    D3D11ScopedComputeBindings& operator=(const D3D11ScopedComputeBindings&) = delete;

    D3D11ScopedComputeBindings(D3D11ScopedComputeBindings&& other) noexcept;
    D3D11ScopedComputeBindings& operator=(D3D11ScopedComputeBindings&& other) noexcept;

    void Restore() noexcept;
};
```

### 動作

1. constructor で binding set が触る range の current compute-stage state を取得
2. previous state を `ComPtr` で保持
3. 指定 binding set を bind
4. destructor または `Restore()` で previous state を復元

```cpp
D3D11ComputeBindingSet bindings;
bindings.SetConstantBuffer(0, constantBuffer.Get());

{
    D3D11ScopedComputeBindings scoped(context, bindings);
    // temporary binding is active
}
// previous binding is restored
```

### Restore は idempotent

`Restore()` は複数回呼んでも二重復元しない設計です。

```cpp
scoped.Restore();
scoped.Restore(); // no-op
```

### Move semantics

`D3D11ScopedComputeBindings` は copy 禁止、move 可能です。

move assignment では、代入先が保持していた previous state を先に restore してから、代入元の restore responsibility を引き継ぎます。

---

## UAV initial count limitation

D3D11 は、現在の append/consume UAV counter 値を取得する API を提供しません。

そのため、`D3D11ScopedComputeBindings::Restore()` は UAV view binding を復元しますが、UAV counter 値そのものを capture / restore することはできません。

実装では、UAV restore 時に `initialCounts = nullptr` を渡し、既存 counter を preserve する方針です。

---

## 典型的な使い方

```cpp
D3D11ComputeBindingSet bindings;
bindings.SetShaderResource(0, inputSrv.Get());
bindings.SetUnorderedAccess(0, outputUav.Get());
bindings.SetConstantBuffer(0, paramsCb.Get());
bindings.SetSampler(0, sampler.Get());

{
    D3D11ScopedComputeBindings scoped(context, bindings);
    pipeline.Bind(context);
    pipeline.Dispatch(context, groupsX, groupsY, groupsZ);
    pipeline.Unbind(context);
}

D3D11UnbindComputeResources(context);
```

---

## サンプル

```text
sample/20_ComputeBindingSet
```

このサンプルは、visible window や file I/O を使わずに、WARP-compatible な buffer / SRV / UAV / sampler を作成し、`D3D11ComputeBindingSet` と `D3D11ScopedComputeBindings` の最小利用例を示します。

---

## 非目標

`D3D11BindingSet` は以下を行いません。

- resource lifetime ownership
- shader reflection による auto binding
- D3D12-style descriptor heap abstraction
- graphics-stage binding set
- material system
- renderer framework
- render graph
- file/media I/O

Graphics-stage binding helper が必要になった場合は、compute-stage helper と混ぜずに、後続バージョンで `D3D11GraphicsBindingSet` のように分ける方針です。
