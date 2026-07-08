# D3D11Helper

**Direct3D 11** の定型処理を薄くラップした **C++17 ヘルパライブラリ**です。

v1.1.0 でモジュール構成を整理し、v1.2.0 で `Texture2D` と CPU メモリ間の transfer API、v1.3.0 で render target / swapchain / resize / present 周辺の Presentation helper、v1.4.0 で compute-stage binding helper、v1.5.0 で Diagnostics helper、v1.6.0 で Copy / Resolve / Mipmap helper、v1.7.0 で advanced View / State helper、v1.8.0 で shared handle / shared texture / keyed mutex / D3D11.4 Fence interop helper を追加しています。

```text
D3D11Foundation   DirectX / DXGI only の基礎 utility
D3D11Core         Device / Immediate Context / Deferred Context / DXGI facade
D3D11Gpu          Resource / View / State / Sampler / Shader / Pipeline / Transfer / Copy / Resolve / Mipmap / Binding
D3D11Presentation RenderTarget / SwapChain / BackBuffer / Resize / Present
D3D11Processing   GPU 画像処理
D3D11Interop      SharedHandle / SharedTexture / KeyedMutex / D3D11.4 Fence / D3D11-D3D12 interop
D3D11Diagnostics  Debug Layer / InfoQueue / LiveObject / DeviceLost / GPU Timer / Profiler
```

旧 `D3D11Framework/*` および一部の旧 `D3D11Core/*` ヘッダは、v1.x 互換のため wrapper として残しています。新規コードでは、上記の新モジュールパスを使うことを推奨します。

> 姉妹プロジェクト [D3D12Helper](https://github.com/Isasa2357/D3D12Helper) と、細部の API ではなく「提供する機能カテゴリ」と「モジュール構成」が揃うことを目標にしています。

---

## Release notes

### v1.11.1 - CMake standalone hardening

- `sample/` 単体から `cmake -S sample -B ...` できる standalone configure fallback を追加。
- `Test/` 単体から `cmake -S Test -B ...` できる standalone configure fallback を追加。
- root からの通常ビルドでは既存の `D3D11Helper::D3D11Helper` target を使い続けるため、従来のビルド経路は維持。
- `CMakeLists.txt` の project version を `1.11.1` に更新。

---

## 特徴

- **DirectX / DXGI 中心** — PNG / JPEG / 動画エンコードなどの file/media I/O は本体に含めず、上位ライブラリへ分離する方針。
- **D3D11Core** — device / immediate context / deferred context / DXGI を束ねる facade。
- **D3D11Gpu** — buffer / texture / view / state / sampler / shader compiler / compute pipeline / graphics pipeline / staging / CPU transfer / copy / resolve / mipmap / binding helper を提供。
- **D3D11Presentation** — offscreen render target、window swapchain、backbuffer RTV、optional depth/stencil、viewport、clear、present、resize を提供。
- **D3D11Diagnostics** — Debug Layer、InfoQueue、LiveObject report、device lost 判定、GPU timestamp timer、single-frame GPU profiler を提供。
- **D3D11Processing** — GPU 上で format conversion、resize、remap、composite、blur、region effect、mask、threshold、pyramid、fused pipeline などを実行。単体 C++ processor はショートカットであり、実務的な連続処理は `shaders/D3D11Processing/*.hlsli` を include した独自 fused HLSL として 1 dispatch にまとめる方針。
- **D3D11Interop** — owned shared handle、shared Texture2D、keyed mutex、D3D11.4 Fence support check、D3D11/D3D12 interop 向け同期補助を提供。
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

## Processing の考え方

`D3D11FormatConverter` や `D3D11Resizer` は、Processing Layer の単体機能をすぐ使うためのショートカットです。Processing Layer の本体は、HLSL 関数ライブラリ、format / plane view 作成、constant buffer、validation、compute pipeline 実行基盤を含むレイヤーです。

複数処理が連続する実アプリでは、ショートカットを何回も呼ぶより、`ProcessingCommon.hlsli` や `ColorSpace.hlsli` を include して、アプリ側の fused shader にまとめることを推奨します。例: `sample/25_ProcessingCustomFusedShader`。

---

## Interop

v1.8.0 以降では、`D3D11Interop` に shared handle / shared texture / keyed mutex / D3D11.4 Fence helper が追加されています。

```cpp
#include <D3D11Helper/D3D11Interop/D3D11Interop.hpp>

D3D11SharedTexture2DDesc desc = {};
desc.width = 1280;
desc.height = 720;
desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
desc.bindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
desc.syncMode = D3D11SharedTextureSyncMode::KeyedMutex;

D3D11SharedTexture2D shared = CreateSharedTexture2DWithHandle(core->GetDevice(), desc);

D3D11KeyedMutex mutex(shared.Get());
mutex.AcquireOrThrow(0, 1000);
// use shared texture
mutex.Release(1);

D3D11FenceSupportInfo fenceSupport = CheckD3D11FenceSupport(
    core->GetDevice(),
    core->GetImmediateContext());

if (fenceSupport.supported) {
    D3D11Fence fence;
    fence.Initialize(core->GetDevice());
    fence.Signal(core->GetImmediateContext(), 1);
    core->Flush();
    fence.CpuWait(1);
}
```

`D3D11SharedHandle` は NT shared handle の所有を RAII 化します。raw `HANDLE` を返す既存 API も互換のため維持しています。

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
