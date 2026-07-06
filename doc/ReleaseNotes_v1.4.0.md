# D3D11Helper v1.4.0 Release Notes

## 概要

`v1.4.0` は **compute-stage binding helper** の追加版です。

D3D11 の compute dispatch 前後で繰り返し発生する SRV / UAV / constant buffer / sampler の binding / unbinding と、previous state の scoped restore を扱う小さな helper を追加しました。

---

## 追加機能

### D3D11ComputeBindingSet

Compute stage の resource binding をまとめる非所有 helper です。

```cpp
D3D11ComputeBindingSet bindings;
bindings.SetShaderResource(0, srv.Get());
bindings.SetUnorderedAccess(0, uav.Get());
bindings.SetConstantBuffer(0, constantBuffer.Get());
bindings.SetSampler(0, sampler.Get());

bindings.Bind(context);
```

### D3D11ScopedComputeBindings

対象 binding set が触る compute slots の previous state を capture し、スコープ終了時に復元する RAII helper です。

```cpp
{
    D3D11ScopedComputeBindings scoped(context, bindings);
    // compute processing
}
```

### Compute-stage bind/unbind helper

```cpp
BindComputeShaderResources(...);
BindComputeUnorderedAccessViews(...);
BindComputeConstantBuffers(...);
BindComputeSamplers(...);

UnbindComputeShaderResources(...);
UnbindComputeUnorderedAccessViews(...);
UnbindComputeConstantBuffers(...);
UnbindComputeSamplers(...);
D3D11UnbindComputeResources(...);
```

---

## テスト

`Test/test_binding_set.cpp` を追加しました。

主なテスト内容:

- `Empty()` / `Clear()`
- slot validation
- helper range validation
- contiguous range holes
- null-entry bind/unbind
- global compute unbind
- scoped restore for SRV / UAV / constant buffer / sampler
- idempotent `Restore()`
- scoped move construction / move assignment

---

## サンプル

`sample/20_ComputeBindingSet` を追加しました。

このサンプルは以下を示します。

- WARP-compatible buffer 作成
- SRV / UAV / constant buffer / sampler 作成
- `D3D11ComputeBindingSet` への登録
- `D3D11ScopedComputeBindings` による scoped binding
- `D3D11UnbindComputeResources` による明示 unbind

visible window、file I/O、外部依存はありません。

---

## 互換性

既存 API の破壊的変更はありません。

`D3D11Gpu.hpp` に `D3D11BindingSet.hpp` が追加されたため、新規 code は umbrella include だけで binding helper を利用できます。

---

## 注意点

### Binding set は resource を所有しない

`D3D11ComputeBindingSet` は raw pointer を保持します。登録した SRV / UAV / buffer / sampler の lifetime は呼び出し側が管理してください。

### UAV counter は完全復元できない

D3D11 は現在の append/consume UAV counter 値を取得する API を提供しません。`D3D11ScopedComputeBindings::Restore()` は UAV view binding を復元しますが、counter 値そのものは capture / restore しません。

### Compute-stage のみ

v1.4.0 は compute-stage binding helper のみを対象にしています。graphics-stage binding helper は後続バージョンで分けて扱う想定です。

---

## 非目標

v1.4.0 では以下を追加しません。

- graphics-stage binding set
- descriptor heap abstraction
- shader reflection based auto binding
- resource lifetime tracking
- material system
- renderer framework
- render graph
- file/media I/O
