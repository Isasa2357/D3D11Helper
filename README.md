# D3D11Helper

**Direct3D 11** の定型処理を薄くラップした **C++17 ヘルパライブラリ**です。

v1.1.0 でモジュール構成を整理し、v1.2.0 で `Texture2D` と CPU メモリ間の transfer API、v1.3.0 で render target / swapchain / resize / present 周辺の Presentation helper、v1.4.0 で compute-stage binding helper、v1.5.0 で Diagnostics helper、v1.6.0 で Copy / Resolve / Mipmap helper、v1.7.0 で advanced View / State helper を追加しています。

```text
D3D11Foundation   DirectX / DXGI only の基礎 utility
D3D11Core         Device / Immediate Context / Deferred Context / DXGI facade
D3D11Gpu          Resource / View / State / Sampler / Shader / Pipeline / Transfer / Copy / Resolve / Mipmap / Binding
D3D11Presentation RenderTarget / SwapChain / BackBuffer / Resize / Present
D3D11Processing   GPU 画像処理
D3D11Interop      SharedResource / D3D11.4 Fence / D3D11-D3D12 interop
D3D11Diagnostics  Debug Layer / InfoQueue / LiveObject / DeviceLost / GPU Timer / Profiler
```

旧 `D3D11Framework/*` および一部の旧 `D3D11Core/*` ヘッダは、v1.x 互換のため wrapper として残しています。新規コードでは、上記の新モジュールパスを使うことを推奨します。

> 姉妹プロジェクト [D3D12Helper](https://github.com/Isasa2357/D3D12Helper) と、細部の API ではなく「提供する機能カテゴリ」と「モジュール構成」が揃うことを目標にしています。

---

## 特徴

- **DirectX / DXGI 中心** — PNG / JPEG / 動画エンコードなどの file/media I/O は本体に含めず、上位ライブラリへ分離する方針。
- **D3D11Core** — device / immediate context / deferred context / DXGI を束ねる facade。
- **D3D11Gpu** — buffer / texture / view / state / sampler / shader compiler / compute pipeline / graphics pipeline / staging / CPU transfer / copy / resolve / mipmap / binding helper を提供。
- **D3D11Presentation** — offscreen render target、window swapchain、backbuffer RTV、optional depth/stencil、viewport、clear、present、resize を提供。
- **D3D11Diagnostics** — Debug Layer、InfoQueue、LiveObject report、device lost 判定、GPU timestamp timer、single-frame GPU profiler を提供。
- **D3D11Processing** — GPU 上で format conversion、resize、remap、composite、blur、region effect、mask、threshold、pyramid、fused pipeline などを実行。
- **D3D11Interop** — D3D11/D3D12 共有リソースと D3D11.4 Fence 同期。
- **D3D12Helper と対称的な設計** — D3D11 固有の自然な API を保ちつつ、モジュールと機能カテゴリを合わせる。

---

## 基本 include

```cpp
#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
#include <D3D11Helper/D3D11Presentation/D3D11Presentation.hpp>
#include <D3D11Helper/D3D11Diagnostics/D3D11Diagnostics.hpp>
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

D3D11Resource texture = CreateTexture2DFromCpuImage(*core, image);
D3D11CpuImage readback = ReadbackTexture2DToCpuImage(*core, texture);
```

D3D11Helper 本体は PNG / JPEG / MP4 / NVENC / Media Foundation などの file/media I/O を持ちません。上位ライブラリは `D3D11CpuImage` を境界として実装します。

---

## View / State helpers

v1.7.0 以降では、`D3D11Gpu` に advanced view helper と state preset helper が追加されています。

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>

D3D11Texture2DArrayViewDesc arrayView = {};
arrayView.firstArraySlice = 0;
arrayView.arraySize = 2;
auto srv = CreateTexture2DArraySrv(core->GetDevice(), textureArray.Get(), arrayView);

auto depthDsv = CreateDepthTexture2DDsv(core->GetDevice(), depthTexture.Get());
auto depthSrv = CreateDepthTexture2DSrv(core->GetDevice(), depthTexture.Get());

auto sampler = CreateSamplerState(core->GetDevice(), StatePresets::SamplerLinearClamp());
auto rasterizer = CreateRasterizerState(core->GetDevice(), StatePresets::RasterizerCullBack());
auto blend = CreateBlendState(core->GetDevice(), StatePresets::BlendAlpha());
auto depth = CreateDepthStencilState(core->GetDevice(), StatePresets::DepthDefault());
```

`D3D11View` は Texture2D array / cube / cube array / depth / typed buffer / structured buffer / raw buffer view を補助します。`D3D11State` は sampler / rasterizer / blend / depth-stencil state descriptor preset と state object 作成 helper を提供します。
---

## Copy / Resolve / Mipmap

v1.6.0 以降では、D3D11 resource 間の基本的な copy、MSAA resolve、automatic mip generation を `D3D11Gpu` で扱えます。

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>

CopyTexture2D(core->GetImmediateContext(), dstTexture.Get(), srcTexture.Get());

D3D11Texture2DRegion region = {};
region.srcX = 16;
region.srcY = 16;
region.dstX = 0;
region.dstY = 0;
region.width = 128;
region.height = 128;
CopyTexture2DRegion(core->GetImmediateContext(), dstTexture.Get(), srcTexture.Get(), region);

ResolveTexture2D(core->GetImmediateContext(), singleSampleTexture.Get(), msaaTexture.Get());

if (CanGenerateMipsForTexture2D(core->GetDevice(), mipTexture.Get())) {
    auto srv = CreateMipGenerationTexture2DSRV(core->GetDevice(), mipTexture.Get());
    GenerateMips(core->GetImmediateContext(), srv.Get());
}
```

`D3D11Copy` / `D3D11Resolve` / `D3D11Mipmap` は Direct3D 11 の低レベル API を薄く包む helper です。format conversion、CPU readback、file I/O は扱いません。

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

v1.4.0 以降では、compute-stage の SRV / UAV / constant buffer / sampler を `D3D11ComputeBindingSet` でまとめて扱えます。

```cpp
D3D11ComputeBindingSet bindings;
bindings.SetShaderResource(0, srv.Get());
bindings.SetUnorderedAccess(0, uav.Get());
bindings.SetConstantBuffer(0, constantBuffer.Get());
bindings.SetSampler(0, sampler.Get());

{
    D3D11ScopedComputeBindings scoped(core->GetImmediateContext(), bindings);
    // dispatch...
}

D3D11UnbindComputeResources(core->GetImmediateContext());
```

binding set は non-owning です。登録した view / buffer / sampler の lifetime は呼び出し側が管理します。

---

## Diagnostics

v1.5.0 以降では、device lost 判定、InfoQueue helper、GPU timer、GPU profiler を `D3D11Diagnostics` で扱えます。

```cpp
#include <D3D11Helper/D3D11Diagnostics/D3D11Diagnostics.hpp>

const auto lost = CheckDeviceLost(core->GetDevice());
if (lost.deviceLost) {
    std::cout << "Device lost: " << lost.name << "\n";
}

D3D11GpuTimer timer;
timer.Initialize(core->GetDevice());
timer.Begin(core->GetImmediateContext());
timer.End(core->GetImmediateContext());
core->Flush();

const auto result = timer.Resolve(core->GetImmediateContext(), true);
if (result.completed && result.valid) {
    std::cout << result.milliseconds << " ms\n";
}
```

`D3D11GpuProfiler` は、1 frame 内の named sample を計測する simple single-frame profiler です。

---

## サンプル

代表的なサンプル:

```text
sample/03_HelloTriangle         low-level swapchain helper example
sample/18_TextureTransfer       Texture2D <-> D3D11CpuImage transfer
sample/19_PresentationWindow    D3D11SwapChain window render-loop sample
sample/20_ComputeBindingSet     compute-stage binding helper sample
sample/21_Diagnostics           device lost / InfoQueue / GPU timer / profiler sample
sample/22_CopyResolveMipmap     copy / resolve / mipmap helper sample
sample/23_ViewState             advanced view / state helper sample
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
- [`doc/D3D11BindingSet.md`](doc/D3D11BindingSet.md)
- [`doc/D3D11TextureTransfer.md`](doc/D3D11TextureTransfer.md)
- [`doc/D3D11Copy.md`](doc/D3D11Copy.md)
- [`doc/D3D11Resolve.md`](doc/D3D11Resolve.md)
- [`doc/D3D11Mipmap.md`](doc/D3D11Mipmap.md)
- [`doc/D3D11View.md`](doc/D3D11View.md)
- [`doc/D3D11State.md`](doc/D3D11State.md)
- [`doc/D3D11Presentation.md`](doc/D3D11Presentation.md)
- [`doc/D3D11Diagnostics.md`](doc/D3D11Diagnostics.md)
- [`doc/D3D11GpuTimer.md`](doc/D3D11GpuTimer.md)
- [`doc/D3D11GpuProfiler.md`](doc/D3D11GpuProfiler.md)
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
GPU profiler file export / Chrome trace export
```

これらは D3D11Helper の上位ライブラリで実装する方針です。
