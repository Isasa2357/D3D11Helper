# D3D11Helper

**Direct3D 11** の定型処理を薄くラップした **C++17 ヘルパライブラリ**です。

v1.1.0 では、従来の `D3D11Core / D3D11Framework / D3D11Processing` という 3 レイヤ構成を、今後の D3D12Helper との機能対応を見据えて、次のモジュール構成へ整理しています。

```text
D3D11Foundation   DirectX / DXGI only の基礎 utility
D3D11Core         Device / Immediate Context / Deferred Context / DXGI facade
D3D11Gpu          Resource / View / Sampler / Shader / Pipeline / Transfer
D3D11Presentation SwapChain / BackBuffer / Present 系
D3D11Processing   GPU 画像処理
D3D11Interop      SharedResource / D3D11.4 Fence / D3D11-D3D12 interop
D3D11Diagnostics  Debug Layer / InfoQueue / LiveObject report
```

旧 `D3D11Framework/*` および一部の旧 `D3D11Core/*` ヘッダは、v1.x 互換のため wrapper として残しています。新規コードでは、上記の新モジュールパスを使うことを推奨します。

> 姉妹プロジェクト [D3D12Helper](https://github.com/Isasa2357/D3D12Helper) と、細部の API ではなく「提供する機能カテゴリ」と「モジュール構成」が揃うことを目標にしています。

---

## 特徴

- **DirectX / DXGI 中心** — PNG / JPEG / 動画エンコードなどのファイル I/O は本体に含めず、上位ライブラリへ分離する方針。
- **D3D11Core** — device / immediate context / deferred context / DXGI を束ねる facade。
- **D3D11Gpu** — buffer / texture / view / sampler / shader compiler / compute pipeline / graphics pipeline / staging readback を提供。
- **D3D11Presentation** — swapchain 作成と backbuffer 取得の入口。Present ループや RTV 運用クラスは後続バージョンで拡張予定。
- **D3D11Processing** — GPU 上で format conversion、resize、remap、composite、blur、region effect、mask、threshold、pyramid、fused pipeline などを実行。
- **D3D11Interop** — D3D11/D3D12 共有リソースと D3D11.4 Fence 同期。
- **D3D11Diagnostics** — Debug Layer / InfoQueue / LiveObject report / DebugName。
- **D3D12Helper と対称的な設計** — D3D11 固有の自然な API を保ちつつ、モジュールと機能カテゴリを合わせる。

---

## アーキテクチャ

```text
+--------------------------------------------------------------+
|  Application / Higher-level libraries                        |
|  Window / Camera / Renderer / Processing / Interop / Encoder |
+--------------------------------------------------------------+
        |                 |                 |
        v                 v                 v
+----------------+ +----------------+ +----------------------+
| Presentation   | | Processing     | | Interop / Diagnostics |
| SwapChain      | | GPU image proc | | Shared / Debug        |
+----------------+ +----------------+ +----------------------+
        |                 |                 |
        +-----------------+-----------------+
                          |
                          v
+--------------------------------------------------------------+
|  D3D11Gpu                                                    |
|    Resource / View / Sampler / Shader / Pipeline / Transfer  |
+--------------------------------------------------------------+
                          |
                          v
+--------------------------------------------------------------+
|  D3D11Core                                                   |
|    Device / Immediate Context / Deferred Context / DXGI      |
+--------------------------------------------------------------+
                          |
                          v
+--------------------------------------------------------------+
|  D3D11Foundation                                             |
|    Common / ThrowIfFailed / FormatUtil / DxgiUtil            |
+--------------------------------------------------------------+
```

詳細は [`doc/Architecture.md`](doc/Architecture.md) を参照してください。

---

## ディレクトリ構成

```text
D3D11Helper/
├── CMakeLists.txt
├── cmake/
├── include/D3D11Helper/
│   ├── D3D11Foundation/     # Common / HRESULT / DXGI utility / format trait
│   ├── D3D11Core/           # Device / Context facade + compatibility wrappers
│   ├── D3D11Gpu/            # Resource / View / Pipeline / Shader / Staging
│   ├── D3D11Framework/      # v1.x compatibility wrappers for old Layer 2 path
│   ├── D3D11Presentation/   # SwapChain / BackBuffer helpers
│   ├── D3D11Processing/     # GPU image processing
│   ├── D3D11Interop/        # SharedResource / Fence interop
│   └── D3D11Diagnostics/    # Debug Layer / InfoQueue / LiveObject
├── shaders/D3D11Processing/
├── src/
├── sample/
├── Test/
└── doc/
```

`D3D11Framework/` は互換パスです。新規コードでは `D3D11Gpu/` と `D3D11Presentation/` を使ってください。

---

## クイックスタート

```cpp
#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>

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

旧 include も v1.x では維持されます。

```cpp
#include <D3D11Helper/D3D11Framework/D3D11Framework.hpp> // compatibility wrapper
```

Processing Layer の最小形:

```cpp
#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
#include <D3D11Helper/D3D11Processing/D3D11Processing.hpp>

using namespace D3D11CoreLib;
using namespace D3D11CoreLib::Processing;

int main() {
    D3D11CoreConfig cfg;
    cfg.allowWarpAdapter = true;
    auto core = D3D11Core::CreateShared(cfg);

    D3D11ProcessingContext processing;
    processing.Initialize(*core, "shaders/D3D11Processing");

    D3D11Resizer resizer;
    resizer.Initialize(processing);

    auto src = CreateTexture2D(*core, 640, 480, DXGI_FORMAT_R8G8B8A8_UNORM,
        D3D11_BIND_SHADER_RESOURCE);
    auto dst = resizer.CreateOutputTexture(*core, 320, 240, DXGI_FORMAT_R8G8B8A8_UNORM);

    ResizeDesc desc;
    desc.filter = ProcessingFilter::Linear;
    resizer.DispatchResize(core->GetImmediateContext(), src, dst, desc);

    core->Flush();
}
```

---

## ビルド

### 必要環境

- Windows 10 / 11
- Visual Studio 2019 以降（MSVC、C++17）
- CMake 3.20 以降
- Direct3D 11 対応 GPU（WARP でも一部テスト可能）
- DXC を使う場合は `dxcompiler.dll` / `dxil.dll`

### CMake ビルド

```bat
cmake -S . -B out/build/default -DD3D11HELPER_BUILD_SAMPLES=ON -DD3D11HELPER_BUILD_TESTS=ON
cmake --build out/build/default --config Release
ctest --test-dir out/build/default -C Release --output-on-failure
```

### オプション

| CMake オプション | デフォルト | 説明 |
|---|---:|---|
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
| 02 | ComputeGrayscale | GPU でグレースケール変換 → CPU readback 検証 | 不要 |
| 03 | HelloTriangle | Win32 ウィンドウに三角形を描画 | あり |
| 04 | BufferCompute | Structured Buffer で GPU 計算 | 不要 |
| 05 | DynamicStreaming | 毎フレーム CPU→GPU テクスチャ更新 | 不要 |
| 06 | SharedResource | 2 デバイス間の共有テクスチャ + D3D11.4 Fence | 不要 |
| 07 | ProcessingFusedConvertResize | NV12 → RGBA resize を fused dispatch | 不要 |
| 08 | ProcessingP010Rgba16 | P010 / RGBA16F の Processing API 使用例 | 不要 |

詳細は [`sample/README.md`](sample/README.md) を参照してください。

---

## テスト

```bat
ctest --test-dir out/build/default -C Debug --output-on-failure
```

Processing のみ実行する場合:

```bat
ctest --test-dir out/build/default -C Debug -R Processing --output-on-failure
```

---

## D3D12Helper との対応

| 機能カテゴリ | D3D12Helper | D3D11Helper |
|---|---|---|
| Foundation | Common / HRESULT / DXGI utility | Common / HRESULT / DXGI utility |
| Core | Device / Queue / Fence / CommandContext | Device / Immediate Context / Deferred Context |
| GPU object | Resource / Descriptor / Upload / Readback / Pipeline | Resource / View object / Staging / Pipeline |
| Presentation | SwapChain / BackBuffer | SwapChain / BackBuffer |
| Processing | Record commands into command list | Dispatch on `ID3D11DeviceContext*` |
| Interop | SharedResource / Fence | SharedResource / D3D11.4 Fence |
| Diagnostics | Debug Layer / InfoQueue / GPU validation | Debug Layer / InfoQueue / LiveObject report |

D3D11 側では、D3D12 の Barrier / DescriptorHeap / UploadBuffer を無理に模倣しません。D3D11 の View object、Immediate/Deferred Context、`UpdateSubresource`、staging resource を使って同じ目的を自然に表現します。

---

## ドキュメント

| ファイル | 内容 |
|---|---|
| [`doc/README.md`](doc/README.md) | ドキュメント入口 |
| [`doc/Architecture.md`](doc/Architecture.md) | v1.1.0 モジュール構成 |
| [`doc/D3D11Foundation.md`](doc/D3D11Foundation.md) | Foundation API |
| [`doc/D3D11Core.md`](doc/D3D11Core.md) | Core API |
| [`doc/D3D11Gpu.md`](doc/D3D11Gpu.md) | GPU resource / view / pipeline API |
| [`doc/D3D11Framework.md`](doc/D3D11Framework.md) | 旧 Framework 互換パスの説明 |
| [`doc/D3D11Presentation.md`](doc/D3D11Presentation.md) | Presentation API |
| [`doc/D3D11Processing.md`](doc/D3D11Processing.md) | Processing API |
| [`doc/D3D11Interop.md`](doc/D3D11Interop.md) | Interop API |
| [`doc/D3D11Diagnostics.md`](doc/D3D11Diagnostics.md) | Diagnostics API |
| [`doc/Patterns.md`](doc/Patterns.md) | よくある処理パターン |
| [`sample/README.md`](sample/README.md) | サンプル一覧 |
| [`Test/README.md`](Test/README.md) | テスト構成 |

---

## ライセンス

MIT License
