# D3D11Helper ドキュメント

D3D11Helper は、Direct3D 11 の定型処理（デバイス生成・リソース作成・View 作成・Compute / Graphics パイプライン・シェーダコンパイル・ステージングバッファ・共有リソース・GPU 画像処理）を薄くラップした **Layer 1 / Layer 2 / Layer 3 構成の C++17 ヘルパライブラリ**です。

- すべての型・関数は名前空間 `D3D11CoreLib` に属します。
- Processing Layer の型・関数は `D3D11CoreLib::Processing` に属します。
- テキストファイルは UTF-8 BOM 付きで保存することを推奨します。MSVC で日本語コメントを含む `.hpp` / `.cpp` を扱う場合、BOM なし UTF-8 は環境によって CP932 として解釈されることがあります。
- 対象環境は Windows / MSVC、Direct3D 11 + DXGI 1.6 です。

| ファイル | 内容 |
| --- | --- |
| `README.md`（本書） | 全体像・設計思想・ファイル構成・クイックスタート |
| [`D3D11Core.md`](D3D11Core.md) | Layer 1（`D3D11Core/`）の API リファレンス |
| [`D3D11Framework.md`](D3D11Framework.md) | Layer 2（`D3D11Framework/`）の API リファレンス |
| [`D3D11Processing.md`](D3D11Processing.md) | Layer 3（`D3D11Processing/`）の API リファレンス |
| [`Patterns.md`](Patterns.md) | よくある処理パターン（レシピ集） |

実際に動くコードは [`../sample`](../sample) と [`../Test`](../Test) を参照してください。

---

## 設計思想

### 3 レイヤー構成

```text
+--------------------------------------------------------------+
|  アプリ / 上位サブシステム（Window, Camera, Renderer, ...）  |
+--------------------------------------------------------------+
                  | 1 つの D3D11Core を shared_ptr で共有
                  v
+--------------------------------------------------------------+
|  Layer 3 : D3D11Processing                                   |
|    GPU image processing                                      |
|      FormatConverter / Resizer / Remapper / Compositor       |
|      FusedProcessor                                          |
|      NV12 / P010 plane view, HLSL shader library             |
+--------------------------------------------------------------+
                  | Core& と DeviceContext* を受け取って使う
                  v
+--------------------------------------------------------------+
|  Layer 2 : D3D11Framework                                    |
|    ステートフルな building blocks                            |
|      D3D11Resource / StagingBuffer                           |
|      ComputePipeline / GraphicsPipeline / ShaderCompiler     |
|    D3D11Core& を取る自由関数群                                |
|      CreateBuffer / CreateTexture2D... / UpdateTexture2D      |
|      CreateTexture2DFromSubresources / UpdateTextureSub...    |
|      CreateSrv/Uav/Rtv/Dsv / CreateSampler / SwapChain helper|
+--------------------------------------------------------------+
                  | Core& を受け取って使う
                  v
+--------------------------------------------------------------+
|  Layer 1 : D3D11Core（ファサード）                            |
|    D3D11Core                                                 |
|      +- D3D11DeviceContext (Factory/Adapter/Device/Context)  |
|      +- D3D11Fence (D3D11.4 interop 用)                      |
|    ユーティリティ                                            |
|      ThrowIfFailed / DxgiAdapterSelector / FormatUtil        |
|      D3D11Debug / D3D11SharedResource / DxgiUtil             |
+--------------------------------------------------------------+
```

- **Layer 1（D3D11Core）** は「device / immediate context / DXGI」を束ねるファサードです。
- **Layer 2（D3D11Framework）** は、resource / view / staging / pipeline / shader compiler と、`D3D11Core&` を第 1 引数に取る自由関数群です。
- **Layer 3（D3D11Processing）** は、format conversion / resize / remap / composite / fused pass などの画像処理専用レイヤーです。

### D3D12Helper との対応

D3D12 固有の概念（Barrier / Queue / CommandList / DescriptorHeap / UploadBuffer）は D3D11 では不要です。代わりに D3D11 では Immediate Context / Deferred Context と View object を使い、転送や状態遷移の多くをランタイムに任せます。

| D3D12 | D3D11 | 備考 |
| --- | --- | --- |
| Queue / CommandContext | Immediate Context / Deferred Context | D3D11 はランタイム管理 |
| Barrier | 不要 | リソース状態はランタイムが管理 |
| DescriptorHeap / Allocator | View object | SRV / UAV / RTV / DSV / Sampler を直接作成・バインド |
| UploadBuffer / UploadRing | 初期データ付き生成 / UpdateSubresource / Map | ドライバが転送を管理 |
| RecordUploadTextureSubresources | CreateTexture2DFromSubresources / UpdateTextureSubresources | mip / array subresource 更新 |
| ReadbackBuffer | StagingBuffer | `D3D11_USAGE_STAGING` + CPU read |
| Root Signature + PSO | State Objects | Shader / InputLayout / RS / BS / DSS を個別管理 |
| `Record*` Processing API | `Dispatch*` Processing API | D3D11 は `ID3D11DeviceContext*` に発行 |
| D3D12Fence | D3D11Fence (D3D11.4) | 相互運用時のみ使用 |

### 設計原則

1. **Core& を渡す自由関数** — 上位は `D3D11Core` を 1 つ持ち、自由関数で 1 行呼ぶだけでリソースが作れる。
2. **D3D11 に自然な置き換え** — D3D12 の UploadBuffer / DescriptorHeap を D3D11 に持ち込まず、初期データ付き生成・View object・Context binding で表現する。
3. **Dispatch API** — Processing Layer は `ID3D11DeviceContext*` に対して `Dispatch*` で即時発行する。
4. **move-only** — HANDLE や GPU object を所有するクラスは、必要に応じて move-only にする。
5. **ComPtr<T>** エイリアス — `Microsoft::WRL::ComPtr<T>` を `D3D11CoreLib::ComPtr<T>` として使う。
6. **ThrowIfFailed** — HRESULT 失敗時は式・ファイル・行・メッセージ付きの例外。
7. **汎用 helper で扱わない形式は明示拒否** — planar format などは中途半端に扱わず、Processing Layer で扱う。

---

## ファイル構成

```text
D3D11Helper/
  include/D3D11Helper/
    D3D11Core/          ... Layer 1
      D3D11Common.hpp
      D3D11CoreConfig.hpp
      D3D11Core.hpp / .cpp
      D3D11DeviceContext.hpp / .cpp
      D3D11DeferredContext.hpp / .cpp
      D3D11Debug.hpp / .cpp
      D3D11Fence.hpp / .cpp
      D3D11FormatUtil.hpp / .cpp
      D3D11SharedResource.hpp / .cpp
      DxgiAdapterSelector.hpp / .cpp
      DxgiUtil.hpp / .cpp
      ThrowIfFailed.hpp / .cpp
    D3D11Framework/     ... Layer 2
      D3D11Framework.hpp     (まとめ include)
      D3D11Resource.hpp
      D3D11Helpers.hpp / .cpp
      D3D11StagingBuffer.hpp / .cpp
      D3D11ComputePipeline.hpp / .cpp
      D3D11GraphicsPipeline.hpp / .cpp
      D3D11ShaderCompiler.hpp / .cpp
      D3D11SwapChainHelper.hpp / .cpp
    D3D11Processing/    ... Layer 3
      D3D11Processing.hpp    (まとめ include)
      D3D11ProcessingTypes.hpp / .cpp
      D3D11ProcessingContext.hpp / .cpp
      D3D11ProcessingShaderCache.hpp / .cpp
      D3D11TextureViews.hpp / .cpp
      D3D11FormatConverter.hpp / .cpp
      D3D11Resize.hpp / .cpp
      D3D11Remap.hpp / .cpp
      D3D11Composite.hpp / .cpp
      D3D11FusedPipeline.hpp / .cpp
  shaders/D3D11Processing/  ... Processing HLSL / hlsli
  doc/                ... 本ドキュメント
  sample/             ... サンプルコード
  Test/               ... テスト
```

---

## クイックスタート

```cpp
#include "D3D11Core/D3D11Core.hpp"
#include "D3D11Framework/D3D11Framework.hpp"

using namespace D3D11CoreLib;

int main() {
    auto core = D3D11Core::CreateShared();

    auto tex = CreateTexture2D(*core, 256, 256, DXGI_FORMAT_R8G8B8A8_UNORM,
                               D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
    auto srv = CreateTexture2DSrv(*core, tex);
    auto rtv = CreateTexture2DRtv(*core, tex);

    auto sampler = CreateSampler(*core, MakeLinearClampSamplerDesc());

    core->Flush();
}
```

GPU で Compute を回して結果を CPU に戻す例は [`../sample/02_ComputeGrayscale`](../sample/02_ComputeGrayscale) を、Processing Layer は [`D3D11Processing.md`](D3D11Processing.md) を参照してください。
