# D3D11Helper テスト

機能（suite）ごとに分割したテストです。既存の standalone tests と、`TEST(Suite, Case)` 形式の harness tests があります。

## 主な suite

- `FormatUtil`
- `Helpers`
- `ShaderCompiler`
- `Processing`

`Processing` suite は Layer 3 の GPU 実行結果を readback して検証します。

主な検証:

- `D3D11ProcessingContext` の初期化と capability query
- Processing shader compile
- RGBA copy / resize point の readback 検証
- NV12 → RGBA の readback 検証
- RGBA → NV12 の Y/UV readback 検証
- remap / composite の readback 検証
- fused convert+resize の readback 検証
- P010 / RGBA16F 関連 API

## 実行

```bat
cmake --build out/build/default --config Debug
ctest --test-dir out/build/default -C Debug --output-on-failure
ctest --test-dir out/build/default -C Debug -R Processing --output-on-failure
```
