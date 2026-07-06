# D3D11Helper サンプル

D3D11Helper の使い方を示すサンプル集です。`03_HelloTriangle` 以外はウィンドウ不要で、コンソールから実行できます。

| サンプル | 内容 | 主に使う機能 |
| --- | --- | --- |
| [`01_HelloDevice`](01_HelloDevice) | 最小の初期化。アダプタ名・feature level・基本情報を表示 | `D3D11Core` / `D3D11CoreConfig` / `D3D11DeviceContext` |
| [`02_ComputeGrayscale`](02_ComputeGrayscale) | 画像を GPU でグレースケール変換し、CPU readback で検証 | `D3D11ComputePipeline` / SRV / UAV / `D3D11StagingBuffer` |
| [`03_HelloTriangle`](03_HelloTriangle) | Win32 ウィンドウに三角形を描画 | swapchain / graphics pipeline / render target |
| [`04_BufferCompute`](04_BufferCompute) | GPU buffer compute | buffer / compute shader / staging readback |
| [`05_DynamicStreaming`](05_DynamicStreaming) | dynamic texture streaming | dynamic texture / `Map` / `Unmap` |
| [`06_SharedResource`](06_SharedResource) | shared resource 作成 | shared texture / D3D11.4 fence |
| [`07_ProcessingFusedConvertResize`](07_ProcessingFusedConvertResize) | NV12 → RGBA resize を fused dispatch で実行し、CPU readback で検証 | `D3D11ProcessingContext` / `D3D11FusedProcessor` / NV12 plane view |
| [`08_ProcessingP010Rgba16`](08_ProcessingP010Rgba16) | P010 / RGBA16F の Processing API 使用例 | P010 plane view / `R16G16B16A16_FLOAT` / `D3D11FormatConverter` |

## 位置づけ

- **01 → 02** で、初期化、compute dispatch、CPU readback の基本を確認できます。
- **03** はウィンドウ付きの graphics pipeline サンプルです。
- **04** は texture ではなく buffer compute の使い方を示します。
- **05** は毎フレーム CPU から GPU へ更新する dynamic texture の使い方を示します。
- **06** は D3D11/D3D12 interop で必要になる shared resource の入口です。
- **07** は Processing Layer の fused convert+resize を 1 dispatch で実行する例です。
- **08** は P010 と RGBA16F の format 拡張を使う例です。

API の詳細は [`../doc`](../doc) を、Processing Layer は [`../doc/D3D11Processing.md`](../doc/D3D11Processing.md) を参照してください。

---

## 必要環境

- Windows 10 / 11
- Direct3D 11 対応 GPU（サンプル側では基本的に `allowWarpAdapter = true`）
- Visual Studio 2019 以降（MSVC）
- CMake 3.20 以降
- DXC を使うサンプルは実行時に `dxcompiler.dll`（必要に応じて `dxil.dll`）が必要

---

## ビルド

リポジトリルートからビルドします。

```bat
cmake -S . -B out/build/default -G "Visual Studio 17 2022" -A x64 ^
  -DD3D11HELPER_BUILD_SAMPLES=ON ^
  -DD3D11HELPER_BUILD_TESTS=ON

cmake --build out/build/default --config Release
```

Visual Studio generator の場合、生成物は次のような場所に出ます。

```text
out/build/default/sample/Release/D3D11Sample_01_HelloDevice.exe
out/build/default/sample/Release/D3D11Sample_02_ComputeGrayscale.exe
out/build/default/sample/Release/D3D11Sample_03_HelloTriangle.exe
out/build/default/sample/Release/D3D11Sample_04_BufferCompute.exe
out/build/default/sample/Release/D3D11Sample_05_DynamicStreaming.exe
out/build/default/sample/Release/D3D11Sample_06_SharedResource.exe
out/build/default/sample/Release/D3D11Sample_07_ProcessingFusedConvertResize.exe
out/build/default/sample/Release/D3D11Sample_08_ProcessingP010Rgba16.exe
```

構成や generator によって出力パスは変わります。Visual Studio generator の場合は `Debug` / `Release` などの構成別ディレクトリが作られます。

---

## 実行

```bat
out\build\default\sample\Release\D3D11Sample_01_HelloDevice.exe
out\build\default\sample\Release\D3D11Sample_02_ComputeGrayscale.exe
out\build\default\sample\Release\D3D11Sample_07_ProcessingFusedConvertResize.exe
out\build\default\sample\Release\D3D11Sample_08_ProcessingP010Rgba16.exe
```

`RESULT: OK` が出るサンプルは、GPU 実行・読み戻し・検証まで完了しています。

その他のサンプル:

- `D3D11Sample_03_HelloTriangle.exe` — ウィンドウが開き、三角形が表示されます。GUI 環境が必要です。
- `D3D11Sample_04_BufferCompute.exe` — GPU で buffer compute を実行し、CPU 参照と一致するか検証します。
- `D3D11Sample_05_DynamicStreaming.exe` — dynamic texture streaming を実行します。
- `D3D11Sample_06_SharedResource.exe` — shared resource と fence 関連の基本処理を検証します。
- `D3D11Sample_07_ProcessingFusedConvertResize.exe` — NV12 → RGBA resize を 1 dispatch で実行します。
- `D3D11Sample_08_ProcessingP010Rgba16.exe` — P010 / RGBA16F の Processing API 使用例を検証します。

---

## 自分のプロジェクトへの組み込み

CMake を使わない場合も、基本は同じです。

1. インクルードパスに `include/` と `include/D3D11Helper/` を追加する。
2. `src/*.cpp` を自分のビルドに含める。
3. Processing Layer を使う場合は `shaders/D3D11Processing/` を実行時に参照できる場所へ置く。
4. 次のようにインクルードする。

```cpp
#include "D3D11Core/D3D11Core.hpp"
#include "D3D11Gpu/D3D11Gpu.hpp"
#include "D3D11Processing/D3D11Processing.hpp"
```

リンクするシステムライブラリ（`d3d11` / `dxgi` / `dxguid` / `d3dcompiler` / `dxcompiler`）は `D3D11Common.hpp` の `#pragma comment(lib, ...)` で MSVC 向けに自動指定されます。
