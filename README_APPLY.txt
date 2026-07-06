D3D11Helper Pyramid Processor cumulative patch

Apply:
  Extract this ZIP at the D3D11Helper repository root and overwrite existing files.

Added:
  D3D11PyramidProcessor
    - Downsample2x for RGBA-like textures
    - Upsample2x for RGBA-like textures
    - Point / Linear upsample
    - Clamp / Constant edge handling

Build/test:
  build_and_test.cmd

Build/test/push only on success:
  build_test_and_push_if_passed.cmd
