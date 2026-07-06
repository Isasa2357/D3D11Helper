# D3D11Framework 互換リファレンス

`D3D11Framework/` は v1.0.0 までの Layer 2 パスです。v1.1.0 以降、このパスは **互換 wrapper** として残します。

新規コードでは次を使ってください。

```cpp
#include <D3D11Helper/D3D11Gpu/D3D11Gpu.hpp>
#include <D3D11Helper/D3D11Presentation/D3D11Presentation.hpp>
```

## 旧パスと新パス

| 旧パス | 新パス |
|---|---|
| `D3D11Framework/D3D11Resource.hpp` | `D3D11Gpu/D3D11Resource.hpp` |
| `D3D11Framework/D3D11Helpers.hpp` | `D3D11Gpu/D3D11Helpers.hpp` |
| `D3D11Framework/D3D11StagingBuffer.hpp` | `D3D11Gpu/D3D11StagingBuffer.hpp` |
| `D3D11Framework/D3D11ComputePipeline.hpp` | `D3D11Gpu/D3D11ComputePipeline.hpp` |
| `D3D11Framework/D3D11GraphicsPipeline.hpp` | `D3D11Gpu/D3D11GraphicsPipeline.hpp` |
| `D3D11Framework/D3D11ShaderCompiler.hpp` | `D3D11Gpu/D3D11ShaderCompiler.hpp` |
| `D3D11Framework/D3D11SwapChainHelper.hpp` | `D3D11Presentation/D3D11SwapChainHelper.hpp` |

## 方針

- v1.x では旧 include path を削除しません。
- 旧ヘッダは新ヘッダを include するだけの wrapper です。
- 新規実装・サンプル・ドキュメントは新モジュール名へ移行します。
- v2.0.0 以降で旧パスを削除する可能性があります。
