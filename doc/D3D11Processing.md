# D3D11Processing - Layer 3 Processing API

`D3D11Processing` は、D3D11Helper の Layer 3 として実装された GPU 画像処理レイヤーです。Layer 1 / 2 の `D3D11Core`、`D3D11Resource`、`D3D11ComputePipeline`、`D3D11ShaderCompiler` を土台にして、format conversion、resize、remap、composite、fused convert+resize を提供します。

Layer 1 / 2 は汎用 D3D11 操作を担当し、Processing 固有の format / plane view / HLSL / validation はこの Layer 3 が担当します。

---

## 位置づけ

```text
+--------------------------------------------------------------+
|  Application / Camera / Renderer / Encoder / ML Pipeline      |
+--------------------------------------------------------------+
                     |
                     v
+--------------------------------------------------------------+
|  Layer 3 : D3D11Processing                                    |
|    D3D11ProcessingContext                                     |
|    D3D11TextureViews                                          |
|    D3D11FormatConverter                                       |
|    D3D11Resizer                                               |
|    D3D11Remapper                                              |
|    D3D11Compositor                                            |
|    D3D11FusedProcessor                                        |
|    shaders/D3D11Processing/*.hlsl / *.hlsli                  |
+--------------------------------------------------------------+
                     |
                     v
+--------------------------------------------------------------+
|  Layer 2 : D3D11Framework                                     |
|    Resource / View / Staging / Pipeline / ShaderCompiler      |
+--------------------------------------------------------------+
                     |
                     v
+--------------------------------------------------------------+
|  Layer 1 : D3D11Core                                          |
|    Device / Immediate Context / Deferred Context / DXGI       |
+--------------------------------------------------------------+
```

---

## インクルード

```cpp
#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Framework/D3D11Framework.hpp>
#include <D3D11Helper/D3D11Processing/D3D11Processing.hpp>
```

既存サンプルやテストと同じ短い include path を使う場合は次の形式でもよいです。

```cpp
#include "D3D11Core/D3D11Core.hpp"
#include "D3D11Framework/D3D11Framework.hpp"
#include "D3D11Processing/D3D11Processing.hpp"
```

---

## 初期化

Processing Layer は、D3D11 device そのものではなく、既存の `D3D11Core` と shader directory を受け取ります。

```cpp
using namespace D3D11CoreLib;
using namespace D3D11CoreLib::Processing;

D3D11CoreConfig cfg;
cfg.allowWarpAdapter = true;
auto core = D3D11Core::CreateShared(cfg);

D3D11ProcessingContext processing;
processing.Initialize(*core, "shaders/D3D11Processing");
```

D3D11 では descriptor heap は不要です。SRV / UAV / constant buffer は `ID3D11DeviceContext*` に直接 bind されます。Processing pass は内部で必要な view を作成し、dispatch 後に bind slot を解除します。

---

## 実行 API

D3D12 版は `Record*` で command list に記録しますが、D3D11 版は `ID3D11DeviceContext*` へ compute dispatch を発行するため、API 名は `Dispatch*` です。

- `D3D11FormatConverter::DispatchConvert`
- `D3D11Resizer::DispatchResize`
- `D3D11Remapper::DispatchRemap`
- `D3D11Compositor::DispatchComposite`
- `D3D11FusedProcessor::DispatchConvertResize`

Immediate Context だけでなく、Deferred Context の `ID3D11DeviceContext*` に対しても同じ形で発行できます。

---

## 対応 format

### RGBA-like format

- `DXGI_FORMAT_R8G8B8A8_UNORM`
- `DXGI_FORMAT_B8G8R8A8_UNORM`
- `DXGI_FORMAT_R16G16B16A16_FLOAT`

`B8G8R8A8_UNORM` は typed SRV/UAV の format conversion に任せます。HLSL 側で手動 swizzle しません。

### YUV 4:2:0 format

- `DXGI_FORMAT_NV12`
- `DXGI_FORMAT_P010`

Plane view は Processing Layer 側で作成します。

| format | Y plane view | UV plane view |
| --- | --- | --- |
| `NV12` | `R8_UNORM` | `R8G8_UNORM` |
| `P010` | `R16_UNORM` | `R16G16_UNORM` |

### map texture

- `DXGI_FORMAT_R32G32_FLOAT`

Remap の map texture は、destination pixel ごとの source coordinate を `float2` で持ちます。

---

## D3D11FormatConverter

`D3D11FormatConverter` は format conversion 専用 pass です。リサイズは行いません。`srcRect` と `dstRect` のサイズが異なる場合は例外になります。

対応変換:

- RGBA-like → RGBA-like
- NV12 → RGBA-like
- P010 → RGBA-like
- RGBA-like → NV12
- RGBA-like → P010

```cpp
D3D11FormatConverter converter;
converter.Initialize(processing);

auto dst = converter.CreateOutputTexture(
    *core,
    width,
    height,
    DXGI_FORMAT_R8G8B8A8_UNORM);

FormatConvertDesc desc;
desc.srcFormat = DXGI_FORMAT_NV12;
desc.dstFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
desc.color.srcMatrix = ProcessingColorMatrix::BT709;
desc.color.srcRange = ProcessingColorRange::Full;

converter.DispatchConvert(core->GetImmediateContext(), nv12Texture, dst, desc);
core->Flush();
```

色空間は `ProcessingColorDesc` で指定します。

```cpp
ProcessingColorDesc color;
color.srcMatrix = ProcessingColorMatrix::BT709;
color.srcRange  = ProcessingColorRange::Limited;
color.dstMatrix = ProcessingColorMatrix::BT709;
color.dstRange  = ProcessingColorRange::Full;
```

---

## D3D11Resizer

`D3D11Resizer` は RGBA-like texture の resize pass です。

対応 filter:

- `ProcessingFilter::Point`
- `ProcessingFilter::Linear`

```cpp
D3D11Resizer resizer;
resizer.Initialize(processing);

auto resized = resizer.CreateOutputTexture(
    *core,
    dstWidth,
    dstHeight,
    DXGI_FORMAT_R8G8B8A8_UNORM);

ResizeDesc desc;
desc.filter = ProcessingFilter::Linear;

resizer.DispatchResize(core->GetImmediateContext(), srcRgba, resized, desc);
core->Flush();
```

YUV のまま resize する専用 pass ではありません。YUV → RGBA resize を 1 dispatch で行いたい場合は `D3D11FusedProcessor` を使います。

---

## D3D11Remapper

`D3D11Remapper` は map texture による座標変換 pass です。

対応 input / output:

- RGBA-like → RGBA-like

対応 map:

- `DXGI_FORMAT_R32G32_FLOAT`

coordinate mode:

- `RemapCoordinateMode::AbsolutePixels`
- `RemapCoordinateMode::NormalizedZeroToOne`

border mode:

- `RemapBorderMode::Clamp`
- `RemapBorderMode::Constant`

```cpp
D3D11Remapper remapper;
remapper.Initialize(processing);

auto dst = remapper.CreateOutputTexture(
    *core,
    width,
    height,
    DXGI_FORMAT_R8G8B8A8_UNORM);

RemapDesc desc;
desc.srcFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
desc.dstFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
desc.mapFormat = DXGI_FORMAT_R32G32_FLOAT;
desc.filter = ProcessingFilter::Linear;
desc.coordinateMode = RemapCoordinateMode::AbsolutePixels;
desc.borderMode = RemapBorderMode::Clamp;

remapper.DispatchRemap(core->GetImmediateContext(), src, mapTexture, dst, desc);
core->Flush();
```

OpenCV の remap map を使う場合は、GPU texture に `float2(srcX, srcY)` をアップロードして `R32G32_FLOAT` として渡します。

---

## D3D11Compositor

`D3D11Compositor` は base texture と overlay texture を合成します。

対応 format:

- RGBA-like format

blend mode:

- `CompositeBlendMode::Copy`
- `CompositeBlendMode::AlphaBlend`
- `CompositeBlendMode::PremultipliedAlpha`
- `CompositeBlendMode::Add`

```cpp
D3D11Compositor compositor;
compositor.Initialize(processing);

auto dst = compositor.CreateOutputTexture(
    *core,
    width,
    height,
    DXGI_FORMAT_R8G8B8A8_UNORM);

CompositeDesc desc;
desc.blendMode = CompositeBlendMode::AlphaBlend;
desc.opacity = 1.0f;

compositor.DispatchComposite(core->GetImmediateContext(), base, overlay, dst, desc);
core->Flush();
```

`opacity` は `[0, 1]` の範囲です。範囲外は validation error になります。

---

## D3D11FusedProcessor

`D3D11FusedProcessor` は、複数処理を 1 dispatch にまとめるための fused pass です。現在は convert + resize を対象にしています。

対応 fused pass:

- RGBA-like → RGBA-like resize
- NV12 → RGBA-like resize
- P010 → RGBA-like resize

```cpp
D3D11FusedProcessor fused;
fused.Initialize(processing);

auto dst = fused.CreateOutputTexture(
    *core,
    dstWidth,
    dstHeight,
    DXGI_FORMAT_R8G8B8A8_UNORM);

FusedConvertResizeDesc desc;
desc.srcFormat = DXGI_FORMAT_NV12;
desc.dstFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
desc.filter = ProcessingFilter::Linear;
desc.color.srcMatrix = ProcessingColorMatrix::BT709;
desc.color.srcRange = ProcessingColorRange::Full;

fused.DispatchConvertResize(core->GetImmediateContext(), nv12Texture, dst, desc);
core->Flush();
```

`NV12 -> RGBA -> resize` を 2 pass に分ける場合と比べて、中間 RGBA texture と追加 dispatch を避けられます。

---

## P010 と 16bit / float 出力

P010 は 10bit 有効値を 16bit lane に格納する 4:2:0 format です。Processing Layer では `R16_UNORM` / `R16G16_UNORM` plane view を作成し、HLSL 内で正規化値として扱います。

`R16G16B16A16_FLOAT` は RGBA-like format として扱えます。HDR や ML 前処理など、8bit UNORM より広い範囲を保持したい用途で使えます。

---

## HLSL 構成

Processing shader は `shaders/D3D11Processing/` に配置されます。

```text
shaders/D3D11Processing/
├─ ProcessingCommon.hlsli
├─ ProcessingExtendedCommon.hlsli
├─ ColorSpace.hlsli
├─ ConvertRgbToRgb.hlsl
├─ ConvertNv12ToRgb.hlsl
├─ ConvertRgbToNv12.hlsl
├─ ResizeRgba.hlsl
├─ RemapRgba.hlsl
├─ CompositeRgba.hlsl
├─ FusedRgbToRgbResize.hlsl
└─ FusedYuv420ToRgbResize.hlsl
```

CMake ビルドでは、テスト・サンプル実行時に shader directory が exe 横へコピーされます。直接プロジェクトへ組み込む場合は、実行時に `D3D11ProcessingContext::Initialize()` へ shader directory を渡してください。

---

## サンプル

Processing 関連 sample:

| sample | 内容 |
| --- | --- |
| `07_ProcessingFusedConvertResize` | NV12 → RGBA resize を fused pass で実行し、CPU readback で検証 |
| `08_ProcessingP010Rgba16` | P010 / RGBA16F 関連の Processing API を使う最小例 |

すべてコンソールサンプルです。ウィンドウは不要です。

---

## テスト

Processing 関連テストは `Processing` suite に入ります。

検証範囲:

- 型・rect validation
- capability query
- shader compile
- RGBA / BGRA / RGBA16F / NV12 / P010 view validation
- RGBA copy の readback 検証
- NV12 → RGBA の readback 検証
- RGBA → NV12 の Y/UV plane readback 検証
- P010 → RGBA の readback 検証
- resize point の readback 検証
- remap / composite の readback 検証
- fused convert + resize の readback 検証

```bat
ctest --test-dir out/build/default -C Debug -R Processing --output-on-failure
```

---

## 制約と注意

- Processing pass は compute shader ベースです。graphics pipeline の pixel shader pass ではありません。
- In-place 処理はサポートしません。入力と出力は別 resource にしてください。
- D3D11 は resource state barrier を明示しませんが、GPU 完了待ちや CPU readback 前には必要に応じて `Flush()` や staging readback を使ってください。
- 未対応 format / 未対応機能は fallback せず例外化します。
- `D3D11ProcessingContext` に渡す shader directory は、Processing の lifetime 中に参照可能である必要があります。
- shader file は実行時に読まれます。配布時は `shaders/D3D11Processing` を exe から参照可能な場所へ配置してください。
