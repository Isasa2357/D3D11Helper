D3D11Helper Threshold / Visualization patch files
=================================================

This ZIP is an overlay patch. Extract it at the D3D11Helper repository root and overwrite existing files.

Included cumulative features:
  - D3D11Blurrer
  - D3D11RegionEffect
  - D3D11RegionBlur
  - D3D11ColorAdjuster
  - D3D11KernelFilter
  - D3D11MaskProcessor
  - D3D11ThresholdProcessor

New threshold / visualization features:
  - Threshold
  - RangeThreshold
  - ConfidenceHeatmap
  - ClassColorMap
  - MaskOverlay

Recommended command:
  build_test_and_push_if_passed.cmd

The command builds in parallel, runs ctest in parallel, and pushes only when build and tests pass.


2026-07-06 fix: ThresholdRgba.hlsl / RangeThresholdRgba.hlsl の HLSL 予約語 pass を isPassed に変更しました。
