# D3D11Helper v1.5.0 Release Notes

## 概要

`v1.5.0` は **Diagnostics helper** の追加版です。

`D3D11Diagnostics` に、device lost 判定、InfoQueue helper、GPU timestamp timer、simple single-frame GPU profiler を追加しました。

このバージョンの目的は、D3D11Helper 内で GPU 処理や device state を調べるための **DirectX/DXGI-only な基礎診断 API** を提供することです。

---

## 追加機能

### Device lost helper

```cpp
D3D11DeviceLostInfo;
IsDeviceLost();
DeviceLostReasonName();
CheckDeviceLost();
ThrowIfDeviceLost();
```

`ID3D11Device::GetDeviceRemovedReason()` の結果を読み、代表的な device lost HRESULT を分類します。

### InfoQueue helper

```cpp
TryGetD3D11InfoQueue();
GetD3D11InfoQueue();
GetInfoQueueMessageCount();
ClearInfoQueueMessages();
SetInfoQueueBreakOnSeverity();
AddInfoQueueStorageFilterDenySeverity();
```

Debug Layer / InfoQueue が使える環境では、message count や filter 設定を扱えます。

### GPU timer

```cpp
D3D11GpuTimer;
D3D11GpuTimerResult;
D3D11ScopedGpuTimer;
```

`D3D11_QUERY_TIMESTAMP_DISJOINT` と `D3D11_QUERY_TIMESTAMP` を使った同期 timer です。

### GPU profiler

```cpp
D3D11GpuProfiler;
D3D11GpuProfilerSampleResult;
D3D11ScopedGpuProfileSample;
```

1 frame 内の named samples を測る simple profiler です。

### Tests and sample

追加:

```text
Test/test_diagnostics.cpp
sample/21_Diagnostics
```

---

## 対応範囲

```text
- device lost reason classification
- optional InfoQueue access
- timestamp-query based GPU timer
- simple single-frame named sample profiler
- runtime tests
- console sample
```

---

## 非対応・非目標

```text
- async multi-frame profiler ring
- Chrome trace export
- CSV / JSON output
- UI overlay
- ETW / PIX integration
- vendor-specific GPU counters
- automatic frame pacing analysis
```

これらは、必要になった場合に上位ライブラリまたは後続バージョンで扱います。

---

## 互換性

既存 API の削除・rename はありません。

既存の `D3D11Debug::SetupInfoQueue()` / `D3D11Debug::ReportLiveObjects()` / `D3D11Debug::SetDebugName()` はそのまま利用できます。

---

## 注意

GPU timer / profiler は GPU query に依存します。WARP、driver、debug layer、GPU 負荷状況によって、非常に短い空区間では `milliseconds` が 0 に近い値になることがあります。

テストでは実測値そのものではなく、query の完了性と API の安全性を重視します。
