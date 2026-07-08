# D3D11Processing

`D3D11Processing` は、D3D11Helper の GPU 画像処理モジュールです。

v1.1.0 以降は、旧 Layer 3 という位置づけを保ちつつ、モジュール構成上は `D3D11Core` と `D3D11Gpu` の上に立つ Processing モジュールとして扱います。

## Include

```cpp
#include <D3D11Helper/D3D11Processing/D3D11Processing.hpp>
```

## 位置づけ

```text
Application / Camera / Renderer / ML Pipeline
        |
        v
D3D11Processing
        |
        v
D3D11Gpu
        |
        v
D3D11Core
        |
        v
D3D11Foundation
```

Processing 固有の format / plane view / HLSL / validation はこのモジュールが担当します。  
D3D11 の実行モデルに合わせ、各処理は `ID3D11DeviceContext*` に対して `Dispatch*` で発行します。

## 重要: Processing はショートカット API だけではない

`D3D11FormatConverter`、`D3D11Resizer`、`D3D11Remapper` などの C++ クラスは、Processing Layer の全体像そのものではありません。これらは、よく使う単体処理をすぐ呼ぶための **ショートカット / primitive pass** です。

実務上の Processing Layer は、次の 2 層で使うことを想定しています。

1. **単体処理ショートカット**  
   format conversion、resize、remap、blur などを 1 pass で確認したい場合に `D3D11FormatConverter` や `D3D11Resizer` を使います。これは検証・試作・単純な処理向けです。

2. **HLSL 関数ライブラリを使ったアプリ固有の fused pass**  
   実運用で `NV12 -> RGB -> resize -> mask/effect` のように複数処理が連続する場合は、中間 texture を増やさず、`shaders/D3D11Processing/*.hlsli` の関数や定数レイアウトを include して、自分の compute shader に処理列をまとめる方針です。

つまり、Processing Layer の本体は **C++ wrapper 群だけではなく、HLSL 関数ライブラリとそれを実行するための view / constant / validation / pipeline 基盤** です。単体 C++ processor は、その基盤の上に用意された便利な呼び出し口です。

### 使い分け

| やりたいこと | 推奨 |
| --- | --- |
| RGBA を単純に resize したい | `D3D11Resizer` |
| NV12 を RGBA に変換するだけ | `D3D11FormatConverter` |
| NV12 を RGBA にしながら resize したい | 既存の `D3D11FusedProcessor`、または独自 fused HLSL |
| resize 後に mask / highlight / blur / 独自補正も続けたい | 独自 fused HLSL |
| 性能上、中間 texture / 複数 dispatch を避けたい | 独自 fused HLSL |

### 連続処理の基本形

独自 fused pass は次の流れで作ります。

1. `ProcessingCommon.hlsli`、`ColorSpace.hlsli` などを include する。
2. `ProcessingConstants` と同じ cbuffer レイアウトを使う。
3. `CreateYuv420SrvViewSet`、`CreateRgbaTextureViewSet` などで入力 / 出力 view を作る。
4. `D3D11ProcessingContext::UpdateConstants` で定数を更新する。
5. 自分の `D3D11ComputePipeline` で 1 dispatch にまとめて実行する。

この使い方の例として、`sample/25_ProcessingCustomFusedShader` は、アプリ側 HLSL が `ProcessingCommon.hlsli` / `ColorSpace.hlsli` を include し、`NV12 -> RGB -> resize -> 円形領域外 darken` を 1 dispatch で実行します。

## Shader asset directory

実行時 asset としては、HLSL を helper-specific root の下へ配置してください。

```text
D3D11Helper/
  shaders/
    D3D11Processing/
      *.hlsl
      *.hlsli
```

`shaderDirectory` を省略した場合、`D3D11ProcessingContext` はまず `D3D11Helper/shaders/D3D11Processing` を探します。見つからない場合は、既存アプリとの互換性のために従来の `shaders/D3D11Processing` を fallback として参照します。

D3D11Helper と D3D12Helper を同一アプリで使う場合、両方の HLSL を同じ flat な `shaders/` 直下へコピーしないでください。両 helper は同名 HLSL を持つ可能性があります。

## 主な機能

- `D3D11ProcessingContext`
- `D3D11TextureViews`
- `D3D11FormatConverter`
- `D3D11Resizer`
- `D3D11Remapper`
- `D3D11Compositor`
- `D3D11Blur`
- `D3D11RegionEffect`
- `D3D11RegionBlur`
- `D3D11ColorAdjust`
- `D3D11KernelFilter`
- `D3D11MaskProcessor`
- `D3D11ThresholdProcessor`
- `D3D11PyramidProcessor`
- `D3D11PyramidBlur`
- `D3D11PyramidRegionBlur`
- `D3D11FusedPipeline`
- HLSL library files under `shaders/D3D11Processing/`

## サポート方針

本体は DirectX / DXGI に閉じます。画像ファイル読み書きや動画エンコードは Processing には含めません。  
ファイル I/O が必要な場合は、上位の `D3DTextureIO` / `D3DVideoIO` のようなライブラリで、D3D11Helper の readback / upload / resource helper を使って実装する想定です。
