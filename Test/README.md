# D3D11Helper テスト

機能（suite）ごとに分割したテストです。既存の standalone tests と、`TEST(Suite, Case)` 形式の harness tests があります。外部のテストフレームワーク（GoogleTest / Catch2 等）には依存しません。

## 構成

| ファイル | suite / executable | 内容 | デバイス |
| --- | --- | --- | --- |
| `test_utilities.cpp` | `test_utilities` | 低レベル utility | 不要 |
| `test_core.cpp` | `test_core` | 初期化・アダプタ・context | 必要 |
| `test_buffer.cpp` | `test_buffer` | buffer 生成・更新 | 必要 |
| `test_texture.cpp` | `test_texture` | texture 生成・更新 | 必要 |
| `test_view.cpp` | `test_view` | SRV / UAV / RTV / DSV | 必要 |
| `test_staging.cpp` | `test_staging` | staging readback | 必要 |
| `test_shader_compiler.cpp` | `test_shader_compiler` | shader compile | DXC / D3DCompile |
| `test_compute.cpp` | `test_compute` | compute pipeline | 必要 |
| `test_graphics.cpp` | `test_graphics` | graphics pipeline | 必要 |
| `test_shared_resource.cpp` | `test_shared_resource` | shared resource | 必要 |
| `test_integration.cpp` | `test_integration` | 統合テスト | 必要 |
| `test_multithread.cpp` | `test_multithread` | deferred context / multithread | 必要 |
| `test_FormatUtil.cpp` | `FormatUtil` | format 判定・bpp/Bpp | 不要 |
| `test_Helpers.cpp` | `Helpers` | helper 関数 | 必要 |
| `test_ShaderCompiler.cpp` | `ShaderCompiler` | harness 版 shader compiler 検証 | DXC / D3DCompile |
| `test_Processing.cpp` | `Processing` | Layer 3 Processing の shader compile と GPU readback 数値検証 | 必要 / D3DCompile |

共通ファイル:

- `TestCommon.hpp` — 軽量ハーネス。`TEST`, `CHECK`, `CHECK_EQ`, `CHECK_NOTHROW`, `CHECK_THROWS`, `REQUIRE_CORE` など。
- `main.cpp` — harness tests の entry point。引数なしで全 suite、引数に suite 名を渡すとその suite だけ実行。

---

## Processing suite の検証内容

`Processing` suite は Layer 3 の GPU 実行結果を readback して検証します。

主な検証:

- `D3D11ProcessingContext` の初期化と capability query
- Processing shader compile
- RGBA / BGRA / RGBA16F / NV12 / P010 view 作成
- RGBA copy の readback 検証
- point resize の readback 検証
- NV12 → RGBA の readback 検証
- RGBA → NV12 の Y / UV readback 検証
- remap の readback 検証
- composite alpha blend の readback 検証
- fused convert+resize の readback 検証
- P010 / RGBA16F 関連 API の検証

---

## ビルドと実行

ルートからビルドすると、既定でテストも一緒にビルドされます（`D3D11HELPER_BUILD_TESTS=ON`）。

```bat
cmake -S . -B out/build/default -G "Visual Studio 17 2022" -A x64 ^
  -DD3D11HELPER_BUILD_TESTS=ON

cmake --build out/build/default --config Debug

ctest --test-dir out/build/default -C Debug --output-on-failure
```

特定機能だけ実行する場合:

```bat
ctest --test-dir out/build/default -C Debug -R Processing --output-on-failure
ctest --test-dir out/build/default -C Debug -R ShaderCompiler --output-on-failure
```

実行ファイルを直接呼ぶこともできます。

```bat
out\build\default\Test\Debug\d3d11helper_tests.exe
out\build\default\Test\Debug\d3d11helper_tests.exe Processing
```

Visual Studio generator では、standalone tests は `out/build/default/Test/Debug/` のような構成別ディレクトリに出力されます。

---

## 環境による注意

- GPU が無い環境でも WARP で動きますが、D3D11 デバイスを作れない場合はデバイス依存テストは失敗します。
- `dxcompiler.dll` が見つからない場合、DXC を使うテストは失敗または SKIP 相当の挙動になります。CMake ビルドでは DLL コピー処理を用意しています。
- 一部の typed UAV / planar view が非対応な環境では、対応 format を必要とする Processing test が実行できないことがあります。

---

## テストの追加

既存の standalone test は `add_d3d11_test()` で 1 ファイル 1 executable として追加します。

Harness 形式で追加する場合は、`Test/main.cpp` に `TEST(Suite, Case)` を登録する `.cpp` を追加し、`Test/CMakeLists.txt` の `D3D11HELPER_HARNESS_TEST_SOURCES` と `D3D11HELPER_HARNESS_TEST_SUITES` に追加してください。
