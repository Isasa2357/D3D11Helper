# D3D11Helper Architecture

この文書は D3D11Helper v1.1.0 のモジュール構成を説明します。

## 目的

v1.0.0 までの D3D11Helper は、次の 3 レイヤ構成でした。

```text
Layer 1: D3D11Core
Layer 2: D3D11Framework
Layer 3: D3D11Processing
```

この構成は初期実装には十分でしたが、次の機能を追加していくと分類が曖昧になります。

- SwapChain / RenderTarget / Present
- SharedResource / Fence interop
- Debug / InfoQueue / GPU timing
- Transfer / readback / CPU image
- D3D12Helper との機能対応

そこで v1.1.0 では、旧 3 レイヤをより細かいモジュールへ分割します。

---

## 新モジュール構成

```text
D3D11Foundation
D3D11Core
D3D11Gpu
D3D11Presentation
D3D11Processing
D3D11Interop
D3D11Diagnostics
```

### D3D11Foundation

DirectX / DXGI の基礎 utility です。D3D11 device / context を所有しません。

主な内容:

- `D3D11Common.hpp`
- `ThrowIfFailed.hpp`
- `DxgiUtil.hpp`
- `DxgiAdapterSelector.hpp`
- `D3D11FormatUtil.hpp`

### D3D11Core

Device / Context / DXGI を束ねる実行基盤です。

主な内容:

- `D3D11Core`
- `D3D11CoreConfig`
- `D3D11DeviceContext`
- `D3D11DeferredContext`

D3D11 では D3D12 の queue / command list / allocator を明示管理しないため、Core は device と context を中心に構成します。

### D3D11Gpu

D3D11 の GPU object と操作補助です。

主な内容:

- `D3D11Resource`
- buffer / texture 作成 helper
- SRV / UAV / RTV / DSV 作成 helper
- sampler helper
- `D3D11StagingBuffer`
- `D3D11ComputePipeline`
- `D3D11GraphicsPipeline`
- `D3D11ShaderCompiler`

旧 `D3D11Framework/` の主な移行先です。

### D3D11Presentation

ウィンドウ表示と swapchain 周辺です。

主な内容:

- `D3D11SwapChainHelper`
- `CreateSwapChainForHwnd`
- `GetSwapChainBackBuffer`

Present ループ、resize、backbuffer RTV 管理などの高レベル wrapper は後続バージョンで追加します。

### D3D11Processing

GPU 画像処理です。

主な内容:

- format conversion
- resize
- remap
- composite
- blur
- region effect / region blur
- color adjust
- kernel filter
- mask / threshold
- pyramid / pyramid blur
- fused pipeline

Processing は `D3D11Core` と `D3D11Gpu` の上に立つ上位モジュールです。

### D3D11Interop

D3D11/D3D12 や外部 API 連携に必要な DirectX/DXGI レベルの共有機能です。

主な内容:

- `D3D11SharedResource`
- `D3D11Fence`

CUDA / NVENC / Media Foundation / Varjo など個別外部 API への依存はここには入れません。

### D3D11Diagnostics

診断・デバッグ支援です。

主な内容:

- Debug Layer
- InfoQueue setup
- LiveObject report
- DebugName

GPU timer / frame profiler / device lost helper は後続バージョンで追加予定です。

---

## 依存方向

```text
Application
  |
  +--> D3D11Presentation
  |       |
  |       v
  +--> D3D11Processing
  |       |
  |       v
  +--> D3D11Interop
  |
  +--> D3D11Diagnostics
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

実際には Presentation / Processing / Interop / Diagnostics は用途ごとに選択して使います。循環依存は作りません。

---

## 旧 3 レイヤとの対応

| 旧分類 | 新分類 |
|---|---|
| Layer 1: D3D11Core | D3D11Foundation / D3D11Core / D3D11Interop / D3D11Diagnostics |
| Layer 2: D3D11Framework | D3D11Gpu / D3D11Presentation |
| Layer 3: D3D11Processing | D3D11Processing |

`D3D11Framework/` は削除せず、v1.x では wrapper として維持します。

---

## Include 方針

新規コードでは次を推奨します。

```cpp
#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
#include <D3D11Helper/D3D11Processing/D3D11Processing.hpp>
```

旧コードはそのまま動作します。

```cpp
#include <D3D11Helper/D3D11Framework/D3D11Framework.hpp>
```

---

## File / Media I/O の扱い

D3D11Helper 本体は DirectX / DXGI 中心の低依存ライブラリとして維持します。

本体に含めるもの:

- texture / buffer 作成
- upload / update
- staging / readback
- shared resource
- processing
- diagnostics

本体に含めないもの:

- PNG / JPEG / BMP / DDS file I/O
- MP4 / AVI / H.264 / HEVC encode
- NVENC / AMF / QSV / FFmpeg
- Media Foundation encoder / decoder

これらは `D3DTextureIO` や `D3DVideoIO` のような上位ライブラリで扱う想定です。
