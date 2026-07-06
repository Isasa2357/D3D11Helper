D3D11Helper Processing Layer cumulative patch: Blur + RegionEffect

展開方法:
  1. この zip の中身を D3D11Helper のリポジトリ root に展開してください。
  2. 既存ファイルは上書きしてください。

追加内容:
  - D3D11Blurrer: Gaussian / Box blur
  - D3D11RegionEffect: region / mask effects
      - Circle / Rect
      - Inside / Outside
      - Copy
      - Darken
      - Tint
      - Grayscale
      - Highlight
      - AlphaFade
      - Vignette
  - sample/09_ProcessingBlur
  - sample/10_ProcessingRegionEffect
  - Test/test_ProcessingBlur.cpp
  - Test/test_ProcessingRegionEffect.cpp

ビルドとテスト:
  build_and_test.cmd

今回から cmake build は --parallel を有効化しています。

commit / push:
  git_commit_push.cmd
