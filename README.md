# D3D11Helper

**Direct3D 11** の定型処理を薄くラップした **2 レイヤー構成の C++17 ヘルパライブラリ**です。

デバイス生成・リソース作成・View 作成・Compute / Graphics パイプライン・シェーダコンパイル・ステージングバッファ・共有リソースなど、D3D11 アプリケーションで繰り返し書くコードを自由関数と軽量クラスにまとめています。

> 姉妹プロジェクト [D3D12Helper](https://github.com/Isasa2357/D3D12Helper) と同じ設計思想で、D3D11 で自然に扱える形へ置き換えながら API を対称的に設計しています。

---

## 特徴

- **2 レイヤー分離** — Layer 1（Core: Device / Context / DXGI）と Layer 2（Framework: リソース操作）
- **`D3D11Core&` を取る自由関数** — 1 行呼ぶだけでバッファ / テクスチャ / View が作れる
- **Texture helper** — 単一 subresource の同期 upload に加え、mip / array 向けの subresource 初期化・更新に対応
- **View helper** — Texture / Buffer 用の簡易 helper と、特殊 view / typed view / array view 向けの full-desc helper を提供
- **Sampler helper** — Linear Clamp / Point Clamp の desc 作成と `ID3D11SamplerState` 作成を提供
- **ShaderCompiler** — `.cso` 読み込み、D3DCompile / DXC、includeDirs / defines 対応の汎用 compile API を提供
- **GraphicsPipeline** — VS / PS / InputLayout / State Objects を一括管理。3 段カスタマイズ（かんたん / 上書き / フル制御）
- **ComputePipeline** — Compute Shader の Bind / Dispatch / Unbind
- **D3D11.4 Fence** — D3D12 との相互運用（SharedResource + Fence 同期）
- **ThrowIfFailed** — 式・ファイル・行・メッセージ付きの HRESULT 例外
- **D3D12Helper と対称的な API** — 移植時の認知コストを最小化

---

## アーキテクチャ

```
+--------------------------------------------------------------+
|  アプリ / 上位サブシステム                                   |
+--------------------------------------------------------------+
                  | shared_ptr<D3D11Core> を共有
                  v
+--------------------------------------------------------------+
|  Layer 2 : D3D11Framework                                    |
|    Building blocks                                           |
|      D3D11Resource / StagingBuffer                           |
|      ComputePipeline / GraphicsPipeline / ShaderCompiler     |
|    自由関数 (D3D11Core& を第一引数に取る)                     |
|      CreateBuffer / CreateTexture2D / CreateTexture2DSrv     |
|      CreateSrv / CreateUav / CreateRtv / CreateDsv           |
|      CreateSampler / CreateTexture2DFromSubresources         |
|      SwapChain helper                                        |
+--------------------------------------------------------------+
                  |
                  v
+--------------------------------------------------------------+
|  Layer 1 : D3D11Core                                         |
|    D3D11Core (ファサード)                                    |
|      +- D3D11DeviceContext (Factory / Adapter / Device / Ctx)|
|      +- D3D11Fence (D3D11.4 interop)                         |
|    ユーティリティ                                            |
|      ThrowIfFailed / DxgiAdapterSelector / FormatUtil        |
|      D3D11Debug / D3D11SharedResource / DxgiUtil             |
+--------------------------------------------------------------+
```

---

## ディレクトリ構成

```
D3D11Helper/
├── CMakeLists.txt              # ルートビルド（ライブラリ + サンプル + テスト）
├── cmake/                      # CMake ヘルパモジュール
├── include/D3D11Helper/
│   ├── D3D11Core/              # Layer 1 ヘッダ
│   │   ├── D3D11Common.hpp
│   │   ├── D3D11Core.hpp
│   │   ├── D3D11CoreConfig.hpp
│   │   ├── D3D11DeviceContext.hpp
│   │   ├── D3D11Debug.hpp
│   │   ├── D3D11Fence.hpp
│   │   ├── D3D11FormatUtil.hpp
│   │   ├── D3D11SharedResource.hpp
│   │   ├── DxgiAdapterSelector.hpp
│   │   ├── DxgiUtil.hpp
│   │   └── ThrowIfFailed.hpp
│   └── D3D11Framework/         # Layer 2 ヘッダ
│       ├── D3D11Framework.hpp  # まとめ include
│       ├── D3D11Resource.hpp
│       ├── D3D11Helpers.hpp
│       ├── D3D11StagingBuffer.hpp
│       ├── D3D11ComputePipeline.hpp
│       ├── D3D11GraphicsPipeline.hpp
│       ├── D3D11ShaderCompiler.hpp
│       └── D3D11SwapChainHelper.hpp
├── src/                        # 実装 (.cpp)
├── Test/                       # テスト
├── sample/                     # サンプルコード
│   ├── 01_HelloDevice/
│   ├── 02_ComputeGrayscale/
│   ├── 03_HelloTriangle/
│   ├── 04_BufferCompute/
│   ├── 05_DynamicStreaming/
│   └── 06_SharedResource/
└── doc/                        # API リファレンス・使用パターン
```

---

## クイックスタート

```cpp
#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Framework/D3D11Framework.hpp>

using namespace D3D11CoreLib;

int main() {
    auto core = D3D11Core::CreateShared();

    auto tex = CreateTexture2D(*core, 256, 256, DXGI_FORMAT_R8G8B8A8_UNORM,
                               D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
    auto srv = CreateTexture2DSrv(*core, tex);
    auto rtv = CreateTexture2DRtv(*core, tex);

    auto samplerDesc = MakeLinearClampSamplerDesc();
    auto sampler = CreateSampler(*core, samplerDesc);

    core->Flush();
}
```

---

## ビルド

### 必要環境

- **Windows 10 / 11**
- **Visual Studio 2019 以降**（MSVC、C++17）
- **CMake 3.20 以降**
- Direct3D 11 対応 GPU（WARP でも動作可）
- DXC を使う場合は `dxcompiler.dll` / `dxil.dll`

### CMake ビルド

```bat
cmake -S . -B out/build/default -DD3D11HELPER_BUILD_SAMPLES=ON -DD3D11HELPER_BUILD_TESTS=ON
cmake --build out/build/default --config Release
ctest --test-dir out/build/default -C Release --output-on-failure
```

### オプション

| CMake オプション | デフォルト | 説明 |
|---|---|---|
| `D3D11HELPER_BUILD_SAMPLES` | `ON` | サンプルをビルドする |
| `D3D11HELPER_BUILD_TESTS` | `ON` | テストをビルドする |
| `D3D11HELPER_ENABLE_WARNINGS` | `ON` | コンパイラ警告を有効にする |
| `D3D11HELPER_DXC_RUNTIME_DIR` | 空 | `dxcompiler.dll` / `dxil.dll` を含むディレクトリ |
| `D3D11HELPER_COPY_DXC_RUNTIME` | `ON` | DXC ランタイム DLL を実行ファイル横へコピーする |

---

## サンプル

| # | 名前 | 概要 | ウィンドウ |
|---|---|---|---|
| 01 | HelloDevice | 初期化 → アダプタ情報表示 | 不要 |
| 02 | ComputeGrayscale | GPU でグレースケール変換 → CPU リードバック検証 | 不要 |
| 03 | HelloTriangle | Win32 ウィンドウに三角形を描画 | あり |
| 04 | BufferCompute | SAXPY (`y = a*x + y`) を Structured Buffer で GPU 計算 | 不要 |
| 05 | DynamicStreaming | 毎フレーム CPU→GPU テクスチャ更新 (Map/Unmap) | 不要 |
| 06 | SharedResource | 2 デバイス間の共有テクスチャ + D3D11.4 Fence | 不要 |

詳細は [`sample/README.md`](sample/README.md) を参照してください。

---

## D3D12Helper との対応

D3D12 固有の概念は D3D11 では不要になるため、D3D11 側では同じ目的をより直接的な API に置き換えています。

| D3D12 | D3D11 | 備考 |
|---|---|---|
| Command Queue / List / Allocator | Immediate Context / Deferred Context | D3D11 はランタイム管理 |
| Barrier (`ResourceBarrier`) | 不要 | リソース状態はランタイムが管理 |
| Descriptor Heap / Allocator | View object | `ID3D11ShaderResourceView` などを直接作成・バインド |
| Root Signature | 不要 | SRV / UAV / CB / Sampler を context に直接バインド |
| Pipeline State Object (PSO) | State Objects | RS / BS / DSS / Shader / InputLayout を個別管理 |
| Upload Buffer / Upload Ring | `CreateTexture2D` 初期データ / `UpdateSubresource` / `Map` | ドライバが転送を管理 |
| `RecordUploadTextureSubresources` | `CreateTexture2DFromSubresources` / `UpdateTextureSubresources` | mip / array subresource 更新 |
| Readback Buffer | Staging Buffer | `D3D11_USAGE_STAGING` |
| `D3D12Fence` | `D3D11Fence` (D3D11.4) | 相互運用時のみ使用 |

---

## ドキュメント

| ファイル | 内容 |
|---|---|
| [`doc/README.md`](doc/README.md) | 全体像・設計思想・ファイル構成 |
| [`doc/D3D11Core.md`](doc/D3D11Core.md) | Layer 1 API リファレンス |
| [`doc/D3D11Framework.md`](doc/D3D11Framework.md) | Layer 2 API リファレンス |
| [`doc/Patterns.md`](doc/Patterns.md) | よくある処理パターン（レシピ集） |

---

## ライセンス

MIT License
