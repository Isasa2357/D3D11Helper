D3D11Helper ColorAdjust + KernelFilter patch files
==================================================

This ZIP is a cumulative Processing Layer patch. It contains the previous Blur, RegionEffect, and RegionBlur additions plus the new ColorAdjust and KernelFilter additions.

Apply:
  1. Extract this ZIP at the D3D11Helper repository root.
  2. Overwrite existing files when prompted.
  3. Run build_and_test.cmd.

New ColorAdjust API:
  D3D11ColorAdjuster
    - RGBA-like input/output.
    - brightness
    - contrast
    - gamma
    - saturation

New KernelFilter API:
  D3D11KernelFilter
    - RGBA-like input/output.
    - Custom3x3 convolution kernel.
    - Built-in Sharpen kernel.
    - Built-in EdgeDetect kernel.
    - Clamp or Constant edge handling.

Build/test:
  build_and_test.cmd

Build/test and push only if ctest succeeds:
  build_test_and_push_if_passed.cmd

Manual git command:
  git_commit_push.cmd

The build command uses parallel build:
  cmake --build out/build/default --config Debug --parallel

CTest also runs in parallel:
  ctest --test-dir out/build/default -C Debug --output-on-failure -j %NUMBER_OF_PROCESSORS%
