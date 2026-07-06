D3D11Helper pyramid accelerated blur cumulative patch.

Extract this ZIP at the D3D11Helper repository root and overwrite existing files.

Then run:
  build_test_and_push_if_passed.cmd

This adds D3D11PyramidBlur and D3D11PyramidRegionBlur. They use D3D11PyramidProcessor for downsample/upsample and D3D11Blurrer at low resolution to accelerate large blur and region blur effects.
