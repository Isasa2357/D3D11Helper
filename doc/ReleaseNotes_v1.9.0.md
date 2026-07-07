# D3D11Helper v1.9.0 Release Notes

## Summary

v1.9.0 adds packaging and install support with package smoke tests.

## Added

- Install rules for the D3D11Helper target.
- Installed public headers.
- Installed processing shader assets.
- CMake package config files.
- Exported `D3D11Helper::D3D11Helper` target for `find_package` users.
- `D3D11HELPER_ENABLE_PACKAGE_SMOKE_TESTS` option.
- `PackageSmoke` CTest entry.
- Minimal installed-package consumer project.
- Packaging documentation.

## Notes

Install rules and package smoke tests are enabled by default only when D3D11Helper is configured as the top-level project. When consumed by `add_subdirectory` or `FetchContent`, they are disabled by default.
