# D3D11Helper

**Direct3D 11** の定型処理を薄くラップした **C++17 ヘルパライブラリ**です。

v1.13.0では、D3D11に自然な形で汎用GPU基盤を拡張しました。NV12/P010対応YUV HLSL primitive、非所有`D3D11ResourceView`、Texture2D validation、Scoped staging map、D3D11.4 interop向けFence Pointを追加しています。

```text
D3D11Foundation   DirectX / DXGI only の基礎utility
D3D11Core         Device / Immediate Context / Deferred Context / DXGI facade
D3D11Gpu          Resource / View / Validation / State / Pipeline / Transfer / Copy / Resolve / Mipmap
D3D11Presentation RenderTarget / SwapChain / BackBuffer / Resize / Present
D3D11Processing   GPU画像処理と再利用可能なHLSL関数ライブラリ
D3D11Interop      SharedHandle / SharedTexture / KeyedMutex / D3D11.4 Fence / Fence Point
D3D11Diagnostics  Debug Layer / InfoQueue / LiveObject / DeviceLost / GPU Timer / Profiler
```

旧`D3D11Framework/*`および一部の旧`D3D11Core/*`ヘッダは、v1.x互換のためwrapperとして残しています。新規コードでは上記のモジュールパスを推奨します。

> 姉妹プロジェクト[D3D12Helper](https://github.com/Isasa2357/D3D12Helper)と、APIの機械的な一致ではなく、提供する機能カテゴリとモジュール構成を揃える方針です。

---

## Release notes

### v1.13.0 - Generic GPU foundation

- NV12/P010、BT.601/709/2020、Full/Limited、Point/Linear対応の`YuvPrimitives.hlsli`を追加。
- 外部所有`ID3D11Resource`をAddRefせず参照する`D3D11ResourceView`を追加。
- 全Processing processorに別名`*View` dispatch経路を追加。
- Texture2Dのdevice、size、format、mip、array、sample、usage、flagsをまとめて検証するAPIを追加。
- move-only `D3D11MappedSubresource`と`D3D11StagingBuffer::MapScoped()`を追加。
- D3D11.4 / D3D12 interop向け`D3D11FencePoint`を追加。
- CPU/GPU golden test、planar storage readback、COM参照数、例外経路、install/package smoke testを拡充。
- 既存`Dispatch*`、`Map()` / `Unmap()`、`Signal()` / `GpuWait()` / `CpuWait()`のsignatureと挙動を維持。

詳細は[`doc/ReleaseNotes_v1.13.0.md`](doc/ReleaseNotes_v1.13.0.md)を参照してください。

---

## 特徴

- **DirectX / DXGI中心** — PNG、JPEG、動画codec、Media Foundation、NVENCなどのfile/media I/Oは上位ライブラリへ分離。
- **D3D11Core** — device、immediate context、deferred context、DXGIを束ねるfacade。
- **D3D11Gpu** — resource、view、validation、state、shader、pipeline、staging、CPU transfer、copy、resolve、mipmap、binding helper。
- **D3D11Presentation** — offscreen render target、window swapchain、backbuffer、depth/stencil、viewport、present、resize。
- **D3D11Diagnostics** — Debug Layer、InfoQueue、LiveObject report、device lost判定、GPU timer、profiler。
- **D3D11Processing** — format conversion、resize、remap、composite、blur、region effect、mask、threshold、pyramid、fused pipeline。
- **D3D11Interop** — shared handle、shared texture、keyed mutex、D3D11.4 Fence、D3D11/D3D12同期。

---

## 基本include

```cpp
#include <D3D11Helper/D3D11Core/D3D11Core.hpp>
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
#include <D3D11Helper/D3D11Presentation/D3D11Presentation.hpp>
#include <D3D11Helper/D3D11Diagnostics/D3D11Diagnostics.hpp>
#include <D3D11Helper/D3D11Processing/D3D11Processing.hpp>
#include <D3D11Helper/D3D11Interop/D3D11Interop.hpp>
```

---

## Non-owning Resource View

`D3D11ResourceView`はraw `ID3D11Resource*`を保持する非所有viewです。生成、コピー、代入、破棄では`AddRef` / `Release`を行いません。

```cpp
D3D11ResourceView srcView(externalSrcTexture);
D3D11ResourceView dstView(externalDstTexture);

D3D11FormatConverter converter;
converter.Initialize(processingContext);
converter.DispatchConvertView(
    core->GetImmediateContext(),
    srcView,
    dstView,
    convertDesc);
```

既存の所有型`D3D11Resource`と既存`Dispatch*` APIはそのまま利用できます。新APIは同名overloadではなく`*View`という別名なので、既存のaddress-of expressionを曖昧にしません。

Immediate Context利用時も、GPUがresourceを利用している期間は外部ownerがresourceを保持してください。Deferred Contextではcommand listの実行完了まで保持が必要です。

---

## Texture2D validation

```cpp
D3D11Texture2DRequirement requirement;
requirement.device = core->GetDevice();
requirement.width = 1920;
requirement.height = 1080;
requirement.format = DXGI_FORMAT_NV12;
requirement.mipLevels = 1;
requirement.arraySize = 1;
requirement.sampleCount = 1;
requirement.requiredBindFlags = D3D11_BIND_SHADER_RESOURCE;

D3D11ValidationResult result = ValidateTexture2DView(view, requirement);
if (!result) {
    std::cerr << result.Message() << '\n';
}
```

throwing版の`ValidateTexture2DViewOrThrow()`と、所有`D3D11Resource`用のconvenience wrapperも提供します。

---

## Scoped staging map

既存のmanual `Map()` / `Unmap()`に加えて、RAIIで自動`Unmap()`する`MapScoped()`を利用できます。

```cpp
D3D11StagingBuffer staging;
staging.InitializeAsTexture2D(
    core->GetDevice(), width, height, DXGI_FORMAT_R8G8B8A8_UNORM);

core->GetImmediateContext()->CopyResource(staging.Get(), sourceTexture);

{
    auto mapped = staging.MapScoped(core->GetImmediateContext());
    const std::byte* data = mapped.DataAs<std::byte>();
    const UINT rowPitch = mapped.RowPitch();
    const UINT depthPitch = mapped.DepthPitch();
    // read data
} // automatic Unmap
```

`D3D11MappedSubresource`はmove-onlyです。`D3D11StagingBuffer`と`ID3D11DeviceContext`はguardより長く生存させてください。

---

## YUV HLSL primitives

`shaders/D3D11Processing/YuvPrimitives.hlsli`は、アプリケーション所有のfused shaderから利用できるformat-awareなHLSL関数群です。

対応内容:

```text
NV12 / P010
BT.601 / BT.709 / BT.2020
Full / Limited Range
Point / Linear resize sampling
Y / UV plane store
```

P010では10-bit video codeが16-bit wordの上位10bitに格納されるため、`code << 6`を明示的に扱います。

```hlsl
#include "ProcessingCommon.hlsli"
#include "YuvPrimitives.hlsli"

float3 rgb = D3D11SampleYuv420RgbLinear(
    YPlane,
    UVPlane,
    destinationPixel,
    sourceOrigin,
    sourceSize,
    scale,
    SrcFormat,
    SrcRange,
    SrcMatrix);
```

単体C++ processorは簡単に使うためのshortcutです。複数処理を連続して行う場合は、HLSL primitiveをincludeした独自shaderへまとめ、1 dispatchに融合することを推奨します。例は`sample/25_ProcessingCustomFusedShader`を参照してください。

---

## Fence Point

D3D11.4 / D3D12 interopでは、Fenceと値をまとめた`D3D11FencePoint`を利用できます。

```cpp
D3D11Fence fence;
fence.Initialize(core->GetDevice());

auto point = fence.SignalPoint(core->GetImmediateContext(), 1);
core->Flush();
point.CpuWait();
```

別のdevice/contextで待つ場合は`GpuWaitPoint()`を利用します。

```cpp
otherFence.GpuWaitPoint(otherContext, point);
```

D3D11 Immediate Contextは単一のordered GPU timelineです。同一Contextで、後続Signalだけが満たす値を先にWaitすると自己deadlockします。Fence PointはD3D11/D3D12 interopまたは独立して進行するcontext間同期を主用途とします。

---

## Texture transfer

```cpp
D3D11CpuImage image = CreateCpuImage(
    640, 480, DXGI_FORMAT_R8G8B8A8_UNORM);

D3D11Resource texture = CreateTexture2DFromCpuImage(*core, image);
D3D11CpuImage readback = ReadbackTexture2DToCpuImage(*core, texture);
```

`D3D11CpuImage`はraw CPU memory containerであり、画像ファイル形式のload/saveは行いません。

---

## Copy / Resolve / Mipmap

```cpp
CopyTexture2D(
    core->GetImmediateContext(), dstTexture.Get(), srcTexture.Get());

ResolveTexture2D(
    core->GetImmediateContext(), singleSampleTexture.Get(), msaaTexture.Get());

if (CanGenerateMipsForTexture2D(core->GetDevice(), mipTexture.Get())) {
    auto srv = CreateMipGenerationTexture2DSRV(
        core->GetDevice(), mipTexture.Get());
    GenerateMips(core->GetImmediateContext(), srv.Get());
}
```

---

## Presentation

```cpp
D3D11SwapChainDesc desc;
desc.hwnd = hwnd;
desc.width = 1280;
desc.height = 720;
desc.createDepthStencil = true;

auto swapChain = CreateSwapChain(*core, desc);
swapChain.Clear(core->GetImmediateContext());
swapChain.BindAndSetViewport(core->GetImmediateContext());
// draw
swapChain.Present(1, 0);
```

---

## サンプル

代表的なサンプル:

```text
sample/03_HelloTriangle
sample/07_ProcessingFusedConvertResize
sample/08_ProcessingP010Rgba16
sample/18_TextureTransfer
sample/19_PresentationWindow
sample/20_ComputeBindingSet
sample/21_Diagnostics
sample/22_CopyResolveMipmap
sample/23_ViewState
sample/24_Interop
sample/25_ProcessingCustomFusedShader
```

---

## ビルド

```bat
cmake -S . -B out/build/default -G "Visual Studio 17 2022" -A x64
cmake --build out/build/default --config Debug --parallel
ctest --test-dir out/build/default -C Debug --parallel --output-on-failure
```

またはリポジトリ付属の`build_and_test.cmd`を利用できます。

---

## ドキュメント

- [`doc/README.md`](doc/README.md)
- [`doc/Architecture.md`](doc/Architecture.md)
- [`doc/D3D11Gpu.md`](doc/D3D11Gpu.md)
- [`doc/D3D11Processing.md`](doc/D3D11Processing.md)
- [`doc/D3D11TextureTransfer.md`](doc/D3D11TextureTransfer.md)
- [`doc/D3D11Presentation.md`](doc/D3D11Presentation.md)
- [`doc/D3D11Diagnostics.md`](doc/D3D11Diagnostics.md)
- [`doc/GenericGpuFoundationD3D11FinalAudit.md`](doc/GenericGpuFoundationD3D11FinalAudit.md)
- [`doc/ReleaseNotes_v1.13.0.md`](doc/ReleaseNotes_v1.13.0.md)
- [`CHANGELOG.md`](CHANGELOG.md)

---

## 非目標

D3D11Helper本体には以下を含めません。

```text
PNG / JPEG / BMP / DDS load/save
MP4 / H.264 / HEVC encode/decode
Media Foundation / NVENC / FFmpeg / OpenCV integration
Application renderer / scene graph / UI framework
GPU profiler file export / Chrome trace export
```

これらはD3D11Helperの上位ライブラリで実装する方針です。
