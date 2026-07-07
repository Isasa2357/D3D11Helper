# D3D11Helper ドキュメント

D3D11Helper は、Direct3D 11 の定型処理を薄くラップした C++17 ヘルパライブラリです。

v1.1.0 でモジュール構成を整理し、v1.2.0 で GPU/CPU texture transfer、v1.3.0 で presentation helper、v1.4.0 で compute binding helper、v1.5.0 で diagnostics helper、v1.6.0 で copy / resolve / mipmap helper、v1.7.0 で advanced view / state helper を追加しています。

```text
D3D11Foundation
D3D11Core
D3D11Gpu
D3D11Presentation
D3D11Processing
D3D11Interop
D3D11Diagnostics
```

`D3D11Framework/` は v1.x 互換用の wrapper として残します。新規コードでは `D3D11Gpu/` と `D3D11Presentation/` と `D3D11Diagnostics/` を使うことを推奨します。

---

## ドキュメント一覧

| ファイル | 内容 |
| --- | --- |
| [`Architecture.md`](Architecture.md) | v1.1.0 以降のモジュール構成と依存方向 |
| [`D3D11Foundation.md`](D3D11Foundation.md) | DirectX/DXGI-only の基礎 utility |
| [`D3D11Core.md`](D3D11Core.md) | device / context facade |
| [`D3D11Gpu.md`](D3D11Gpu.md) | resource / view / state / staging / pipeline / shader compiler / transfer / copy / resolve / mipmap / binding |
| [`D3D11BindingSet.md`](D3D11BindingSet.md) | compute-stage binding helper |
| [`D3D11TextureTransfer.md`](D3D11TextureTransfer.md) | `Texture2D` と `D3D11CpuImage` 間の transfer |
| [`D3D11Copy.md`](D3D11Copy.md) | D3D11 resource / texture / buffer copy helper |
| [`D3D11Resolve.md`](D3D11Resolve.md) | MSAA `Texture2D` resolve helper |
| [`D3D11Mipmap.md`](D3D11Mipmap.md) | automatic mip generation helper |
| [`D3D11View.md`](D3D11View.md) | advanced Texture2D array / cube / depth / buffer view helper |
| [`D3D11State.md`](D3D11State.md) | sampler / rasterizer / blend / depth-stencil state presets |
| [`D3D11Framework.md`](D3D11Framework.md) | 旧 Framework パスの互換説明 |
| [`D3D11Presentation.md`](D3D11Presentation.md) | render target / swapchain / resize / present helper |
| [`D3D11Diagnostics.md`](D3D11Diagnostics.md) | debug layer / InfoQueue / device lost / GPU timer / profiler |
| [`D3D11GpuTimer.md`](D3D11GpuTimer.md) | timestamp query based GPU timer |
| [`D3D11GpuProfiler.md`](D3D11GpuProfiler.md) | simple single-frame GPU profiler |
| [`D3D11Processing.md`](D3D11Processing.md) | GPU 画像処理 |
| [`D3D11Interop.md`](D3D11Interop.md) | shared resource / D3D11.4 fence interop |
| [`Patterns.md`](Patterns.md) | よくある処理パターン |
| [`ReleaseNotes_v1.4.0.md`](ReleaseNotes_v1.4.0.md) | v1.4.0 release notes |
| [`ReleaseNotes_v1.5.0.md`](ReleaseNotes_v1.5.0.md) | v1.5.0 release notes |
| [`ReleaseNotes_v1.6.0.md`](ReleaseNotes_v1.6.0.md) | v1.6.0 release notes |
| [`ReleaseNotes_v1.7.0.md`](ReleaseNotes_v1.7.0.md) | v1.7.0 release notes |
| [`MigrationGuide_v1.4.0.md`](MigrationGuide_v1.4.0.md) | v1.4.0 migration guide |
| [`MigrationGuide_v1.5.0.md`](MigrationGuide_v1.5.0.md) | v1.5.0 migration guide |
| [`MigrationGuide_v1.6.0.md`](MigrationGuide_v1.6.0.md) | v1.6.0 migration guide |
| [`MigrationGuide_v1.7.0.md`](MigrationGuide_v1.7.0.md) | v1.7.0 migration guide |
| [`../sample`](../sample) | サンプルコード |
| [`../Test`](../Test) | テストコード |

---

## 設計原則

1. **DirectX / DXGI に閉じる**  
   D3D11Helper 本体には PNG / JPEG / MP4 / H.264 / NVENC / Media Foundation encoder のような file/media I/O を含めません。上位ライブラリが必要に応じて実装します。

2. **D3D11 に自然な API にする**  
   D3D12 の DescriptorHeap / Barrier / CommandList を D3D11 に無理に持ち込まず、View object、Immediate/Deferred Context、`UpdateSubresource`、staging resource、swapchain/backbuffer wrapper、query object で表現します。

3. **D3D12Helper と機能カテゴリを揃える**  
   内部実装や細部 API は D3D11 / D3D12 で異なってよいですが、Foundation / Core / Gpu / Presentation / Processing / Interop / Diagnostics という分類は揃えます。

4. **v1.x 互換を維持する**  
   旧 include path は wrapper として残します。新規コードは新モジュールパスを使います。

---

## 推奨 include

```cpp
#include <D3D11Helper/D3D11Foundation/D3D11Foundation.hpp>
#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
#include <D3D11Helper/D3D11Presentation/D3D11Presentation.hpp>
#include <D3D11Helper/D3D11Processing/D3D11Processing.hpp>
#include <D3D11Helper/D3D11Interop/D3D11Interop.hpp>
#include <D3D11Helper/D3D11Diagnostics/D3D11Diagnostics.hpp>
```

旧 include も v1.x では有効です。

```cpp
#include <D3D11Helper/D3D11Framework/D3D11Framework.hpp>
```
