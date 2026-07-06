# D3D11Helper ドキュメント

D3D11Helper は、Direct3D 11 の定型処理を薄くラップした C++17 ヘルパライブラリです。

v1.1.0 では、旧 `Layer 1 / Layer 2 / Layer 3` 表現を維持しつつ、今後の機能追加と D3D12Helper との対応を取りやすくするため、次のモジュール構成へ整理しています。

```text
D3D11Foundation
D3D11Core
D3D11Gpu
D3D11Presentation
D3D11Processing
D3D11Interop
D3D11Diagnostics
```

`D3D11Framework/` は v1.x 互換用の wrapper として残します。新規コードでは `D3D11Gpu/` と `D3D11Presentation/` を使うことを推奨します。

---

## ドキュメント一覧

| ファイル | 内容 |
| --- | --- |
| [`Architecture.md`](Architecture.md) | v1.1.0 のモジュール構成と依存方向 |
| [`D3D11Foundation.md`](D3D11Foundation.md) | DirectX/DXGI-only の基礎 utility |
| [`D3D11Core.md`](D3D11Core.md) | device / context facade |
| [`D3D11Gpu.md`](D3D11Gpu.md) | resource / view / staging / pipeline / shader compiler |
| [`D3D11Framework.md`](D3D11Framework.md) | 旧 Framework パスの互換説明 |
| [`D3D11Presentation.md`](D3D11Presentation.md) | swapchain / backbuffer helper |
| [`D3D11Processing.md`](D3D11Processing.md) | GPU 画像処理 |
| [`D3D11Interop.md`](D3D11Interop.md) | shared resource / D3D11.4 fence interop |
| [`D3D11Diagnostics.md`](D3D11Diagnostics.md) | debug layer / InfoQueue / live object report |
| [`Patterns.md`](Patterns.md) | よくある処理パターン |
| [`../sample`](../sample) | サンプルコード |
| [`../Test`](../Test) | テストコード |

---

## 設計原則

1. **DirectX / DXGI に閉じる**  
   D3D11Helper 本体には PNG / JPEG / MP4 / H.264 / NVENC / Media Foundation encoder のような file/media I/O を含めません。上位ライブラリが必要に応じて実装します。

2. **D3D11 に自然な API にする**  
   D3D12 の DescriptorHeap / Barrier / CommandList を D3D11 に無理に持ち込まず、View object、Immediate/Deferred Context、`UpdateSubresource`、staging resource で表現します。

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
