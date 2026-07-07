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

## Shader asset directory

実行時 asset としては、HLSL を helper-specific root の下へ配置してください。

```text
D3D11Helper/
  shaders/
    D3D11Processing/
      *.hlsl
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

## サポート方針

本体は DirectX / DXGI に閉じます。画像ファイル読み書きや動画エンコードは Processing には含めません。  
ファイル I/O が必要な場合は、上位の `D3DTextureIO` / `D3DVideoIO` のようなライブラリで、D3D11Helper の readback / upload / resource helper を使って実装する想定です。
