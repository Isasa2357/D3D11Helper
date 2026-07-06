# D3D11Helper v1.4.0 Migration Guide

## 概要

`v1.4.0` は backward-compatible な機能追加です。既存コードの必須変更はありません。

Compute shader 実行前後で SRV / UAV / constant buffer / sampler を直接 `CSSet*` していたコードは、必要に応じて `D3D11ComputeBindingSet` と `D3D11ScopedComputeBindings` に移行できます。

---

## 1. Include

従来どおり GPU umbrella から利用できます。

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
```

個別 include する場合:

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11BindingSet.hpp>
```

---

## 2. 直接 CSSet* していたコード

従来:

```cpp
ID3D11ShaderResourceView* srvs[] = { inputSrv.Get() };
ID3D11UnorderedAccessView* uavs[] = { outputUav.Get() };
ID3D11Buffer* cbs[] = { paramsCb.Get() };
ID3D11SamplerState* samplers[] = { sampler.Get() };

context->CSSetShaderResources(0, 1, srvs);
context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
context->CSSetConstantBuffers(0, 1, cbs);
context->CSSetSamplers(0, 1, samplers);

context->Dispatch(groupsX, groupsY, groupsZ);
```

v1.4.0:

```cpp
D3D11ComputeBindingSet bindings;
bindings.SetShaderResource(0, inputSrv.Get());
bindings.SetUnorderedAccess(0, outputUav.Get());
bindings.SetConstantBuffer(0, paramsCb.Get());
bindings.SetSampler(0, sampler.Get());

bindings.Bind(context);
context->Dispatch(groupsX, groupsY, groupsZ);
bindings.Unbind(context);
```

---

## 3. Previous state を戻したい場合

従来は、呼び出し側で `CSGet*` / `CSSet*` を手書きする必要がありました。

v1.4.0 では `D3D11ScopedComputeBindings` を使えます。

```cpp
{
    D3D11ScopedComputeBindings scoped(context, bindings);
    context->Dispatch(groupsX, groupsY, groupsZ);
}
```

スコープを抜けると、binding set が触った slot range の previous compute-stage binding が復元されます。

---

## 4. 全 compute binding を外したい場合

```cpp
D3D11UnbindComputeResources(context);
```

これは compute-stage の SRV / UAV / constant buffer / sampler をまとめて unbind します。

---

## 5. Lifetime の注意

`D3D11ComputeBindingSet` は resource を所有しません。

次のように、binding set に登録する object は、`Bind()` または `D3D11ScopedComputeBindings` の使用中に生存している必要があります。

```cpp
auto srv = CreateBufferSrv(...);

D3D11ComputeBindingSet bindings;
bindings.SetShaderResource(0, srv.Get());

{
    D3D11ScopedComputeBindings scoped(context, bindings);
    // srv must still be alive here
}
```

---

## 6. UAV counter の注意

D3D11 は現在の append/consume UAV counter 値を取得する API を提供しません。

`D3D11ScopedComputeBindings::Restore()` は UAV view binding を復元しますが、counter 値そのものは capture / restore できません。UAV counter を明示的に初期化したい処理では、`SetUnorderedAccess(slot, uav, initialCount)` で binding 時の initial count を指定してください。

---

## 7. 既存コードをそのまま残してよい場合

以下のような場合、既存の `CSSet*` 直接呼び出しを残しても構いません。

- binding 箇所が非常に少ない
- previous state restore が不要
- 独自の binding policy を持っている
- shader reflection / material system 側で別の仕組みを持っている

`D3D11BindingSet` は薄い helper なので、既存コードの強制移行は不要です。
