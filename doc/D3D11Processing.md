# D3D11Processing - Layer 3 Processing API

`D3D11Processing` は、D3D11Helper の Layer 3 として実装された GPU 画像処理レイヤーです。Layer 1 / 2 の `D3D11Core`、`D3D11Resource`、`D3D11ComputePipeline`、`D3D11ShaderCompiler` を土台にして、format conversion、resize、remap、composite、fused convert+resize を提供します。

## インクルード

```cpp
#include "D3D11Core/D3D11Core.hpp"
#include "D3D11Framework/D3D11Framework.hpp"
#include "D3D11Processing/D3D11Processing.hpp"
```

## 初期化

```cpp
D3D11CoreConfig cfg;
cfg.allowWarpAdapter = true;
auto core = D3D11Core::CreateShared(cfg);

D3D11ProcessingContext processing;
processing.Initialize(*core, "shaders/D3D11Processing");
```

## 実行 API

D3D11 では command list へ記録するのではなく、`ID3D11DeviceContext*` へ compute dispatch を発行します。そのため API 名は `Dispatch*` です。

- `D3D11FormatConverter::DispatchConvert`
- `D3D11Resizer::DispatchResize`
- `D3D11Remapper::DispatchRemap`
- `D3D11Compositor::DispatchComposite`
- `D3D11FusedProcessor::DispatchConvertResize`

## 対応 format

RGBA-like:

- `DXGI_FORMAT_R8G8B8A8_UNORM`
- `DXGI_FORMAT_B8G8R8A8_UNORM`
- `DXGI_FORMAT_R16G16B16A16_FLOAT`

YUV 4:2:0:

- `DXGI_FORMAT_NV12`
- `DXGI_FORMAT_P010`

Plane view は Processing Layer が作成します。

| format | Y plane view | UV plane view |
| --- | --- | --- |
| `NV12` | `R8_UNORM` | `R8G8_UNORM` |
| `P010` | `R16_UNORM` | `R16G16_UNORM` |

## Fused pass

`D3D11FusedProcessor` は `RGBA-like -> RGBA-like resize` と `NV12/P010 -> RGBA-like resize` を 1 dispatch で実行します。`NV12 -> RGBA -> resize` を 2 pass に分ける場合と比べて、中間 texture と追加 dispatch を避けられます。

## テスト

Processing 関連テストは `Processing` suite に入ります。

```bat
ctest --test-dir out/build/default -C Debug -R Processing --output-on-failure
```
