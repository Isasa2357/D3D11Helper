# D3D11Helper

**Direct3D 11** の定型処理を薄くラップした **C++17 ヘルパライブラリ**です。

v1.1.0 でモジュール構成を整理し、v1.2.0 で `Texture2D` と CPU メモリ間の transfer API、v1.3.0 で render target / swapchain / resize / present 周辺の Presentation helper、v1.4.0 で compute-stage resource binding helper を追加しています。

```text
D3D11Foundation   DirectX / DXGI only の基礎 utility
D3D11Core         Device / Immediate Context / Deferred Context / DXGI facade
D3D11Gpu          Resource / View / Sampler / Shader / Pipeline / Transfer / Binding
D3D11Presentation RenderTarget / SwapChain / BackBuffer / Resize / Present
D3D11Processing   GPU 画像処理
D3D11Interop      SharedResource / D3D11.4 Fence / D3D11-D3D12 interop
D3D11Diagnostics  Debug Layer / InfoQueue / LiveObject report
```

旧 `D3D11Framework/*` および一部の旧 `D3D11Core/*` ヘッダは、v1.x 互換のため wrapper として残しています。新規コードでは、上記の新モジュールパスを使うことを推奨します。

> 姉妹プロジェクト [D3D12Helper](https://github.com/Isasa2357/D3D12Helper) と、細部の API ではなく「提供する機能カテゴリ」と「モジュール構成」が揃うことを目標にしています。

---

## 特徴

- **DirectX / DXGI 中心** — PNG / JPEG / 動画エンコードなどの file/media I/O は本体に含めず、上位ライブラリへ分離する方針。
- **D3D11Core** — device / immediate context / deferred context / DXGI を束ねる facade。
- **D3D11Gpu** — buffer / texture / view / sampler / shader compiler / compute pipeline / graphics pipeline / staging / CPU transfer / compute binding を提供。
- **D3D11Presentation** — offscreen render target、window swapchain、backbuffer RTV、optional depth/stencil、viewport、clear、present、resize を提供。
- **D3D11Processing** — GPU 上で format conversion、resize、remap、composite、blur、region effect、mask、threshold、pyramid、fused pipeline などを実行。
- **D3D11Interop** — D3D11/D3D12 共有リソースと D3D11.4 Fence 同期。
- **D3D11Diagnostics** — Debug Layer / InfoQueue / LiveObject report / DebugName。
- **D3D12Helper と対称的な設計** — D3D11 固有の自然な API を保ちつつ、モジュールと機能カテゴリを合わせる。

---

## 基本 include

```cpp
#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
#include <D3D11Helper/D3D11Presentation/D3D11Presentation.hpp>
#include <D3D11Helper/D3D11Processing/D3D11Processing.hpp>
```

---

## 最小例: Core 作成

```cpp
#include <D3D11Helper/D3D11Core/D3D11Core.hpp>

using namespace D3D11CoreLib;

int main() {
    D3D11CoreConfig config;
    config.enableDebugLayer = true;
    config.enableInfoQueue = true;

    auto core = D3D11Core::CreateShared(config);
    core->Flush();
    return 0;
}
```

---

## Texture transfer

v1.2.0 以降では、D3D11 texture と CPU memory の橋渡しとして `D3D11CpuImage` と `D3D11TextureTransfer` を使えます。

```cpp
D3D11CpuImage image = CreateCpuImage(640, 480, DXGI_FORMAT_R8G8B8A8_UNORM);

auto texture = CreateTexture2DFromCpuImage(*core, image);
auto readback = ReadbackTexture2DToCpuImage(*core, texture);
```

D3D11Helper 本体は PNG / JPEG / MP4 / NVENC / Media Foundation などの file/media I/O を持ちません。上位ライブラリは `D3D11CpuImage` を境界として実装します。

---

## Presentation

v1.3.0 以降では、offscreen render target と window swapchain を `D3D11Presentation` で扱えます。

```cpp
D3D11RenderTargetDesc rtDesc = {};
rtDesc.width = 1280;
rtDesc.height = 720;
rtDesc.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
rtDesc.depthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

auto target = CreateRenderTarget(*core, rtDesc);
target.Clear(core->GetImmediateContext());
target.BindAndSetViewport(core->GetImmediateContext());
```

window 表示では `D3D11SwapChain` が backbuffer RTV / optional DSV / resize / present を管理します。

```cpp
D3D11SwapChainDesc scDesc = {};
scDesc.hwnd = hwnd;
scDesc.width = 1280;
scDesc.height = 720;
scDesc.createDepthStencil = true;

auto swapChain = CreateSwapChain(*core, scDesc);

swapChain.Clear(core->GetImmediateContext());
swapChain.BindAndSetViewport(core->GetImmediateContext());
// draw...
swapChain.Present(1, 0);
```

---

## Compute binding

v1.4.0 以降では、compute stage の SRV / UAV / constant buffer / sampler binding を `D3D11ComputeBindingSet` でまとめて扱えます。

```cpp
D3D11ComputeBindingSet bindings;
bindings.SetShaderResource(0, srv.Get());
bindings.SetUnorderedAccess(0, uav.Get());
bindings.SetConstantBuffer(0, constantBuffer.Get());
bindings.SetSampler(0, sampler.Get());

{
    D3D11ScopedComputeBindings scoped(core->GetImmediateContext(), bindings);
    // dispatch or helper processing...
}

D3D11UnbindComputeResources(core->GetImmediateContext());
```

`D3D11ComputeBindingSet` は非所有の raw pointer を保持します。binding される view / buffer / sampler の lifetime は呼び出し側が管理してください。`D3D11ScopedComputeBindings` は、対象範囲の previous state だけを `ComPtr` で保持して復元します。

---

## サンプル

代表的なサンプル:

```text
sample/03_HelloTriangle         low-level swapchain helper example
sample/18_TextureTransfer       Texture2D <-> D3D11CpuImage transfer
sample/19_PresentationWindow    D3D11SwapChain window render-loop sample
sample/20_ComputeBindingSet     Compute-stage binding helper sample
```

---

## ビルド

Visual Studio / CMake を使います。

```bat
cmake -S . -B out/build/default -G "Visual Studio 17 2022" -A x64
cmake --build out/build/default --config Debug --parallel
ctest --test-dir out/build/default -C Debug --output-on-failure
```

または、リポジトリ付属の `build_and_test.cmd` を使えます。

```bat
build_and_test.cmd
```

---

## ドキュメント

詳しくは [`doc/README.md`](doc/README.md) を参照してください。

主要ファイル:

- [`doc/Architecture.md`](doc/Architecture.md)
- [`doc/D3D11Gpu.md`](doc/D3D11Gpu.md)
- [`doc/D3D11TextureTransfer.md`](doc/D3D11TextureTransfer.md)
- [`doc/D3D11BindingSet.md`](doc/D3D11BindingSet.md)
- [`doc/D3D11Presentation.md`](doc/D3D11Presentation.md)
- [`doc/Patterns.md`](doc/Patterns.md)
- [`CHANGELOG.md`](CHANGELOG.md)

---

## 非目標

D3D11Helper 本体には以下を含めません。

```text
PNG / JPEG / BMP / DDS load/save
MP4 / H.264 / HEVC encode/decode
Media Foundation / NVENC / FFmpeg / OpenCV integration
Application renderer / scene graph / UI framework
D3D12-style descriptor heap abstraction
Shader reflection driven auto-binding system
```

これらは D3D11Helper の上位ライブラリで実装する方針です。
