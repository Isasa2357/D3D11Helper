D3D11Helper Processing MaskProcessor cumulative patch

Apply:
  Extract this ZIP at the D3D11Helper repository root and overwrite files.

Added:
  - D3D11MaskProcessor
  - MaskApplyDesc / MaskBlendDesc / MaskCombineDesc / MaskInvertDesc
  - MaskApplyMode / MaskCombineMode / MaskChannel
  - MaskApplyRgba.hlsl
  - MaskBlendRgba.hlsl
  - MaskCombineRgba.hlsl
  - MaskInvertRgba.hlsl
  - sample/14_ProcessingMask
  - Test/test_ProcessingMask.cpp

Supported operations:
  - ApplyAlpha
  - MultiplyRgb
  - MultiplyRgba
  - ReplaceAlpha
  - BlendByMask
  - CombineMasks: Add / Multiply / Max / Min / Subtract
  - InvertMask

Notes:
  - This patch is cumulative over Blur, RegionEffect, RegionBlur, ColorAdjust, and KernelFilter.
  - Mask textures are RGBA-like textures. Select Red / Green / Blue / Alpha / Luma as the mask channel.
  - build_and_test.cmd uses parallel build and parallel ctest.
  - build_test_and_push_if_passed.cmd pushes only when build and ctest succeed.
