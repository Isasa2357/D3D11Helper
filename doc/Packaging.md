# D3D11Helper Packaging

D3D11Helper can be consumed by `add_subdirectory`, `FetchContent`, or installed CMake package config files.

## add_subdirectory

```cmake
add_subdirectory(path/to/D3D11Helper)
target_link_libraries(MyApp PRIVATE D3D11Helper::D3D11Helper)
```

## FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
    D3D11Helper
    GIT_REPOSITORY https://github.com/Isasa2357/D3D11Helper.git
    GIT_TAG main
)
FetchContent_MakeAvailable(D3D11Helper)

target_link_libraries(MyApp PRIVATE D3D11Helper::D3D11Helper)
```

## Install and find_package

```bat
cmake -S . -B out/build/install -G "Visual Studio 17 2022" -A x64 ^
  -DD3D11HELPER_BUILD_SAMPLES=OFF ^
  -DD3D11HELPER_BUILD_TESTS=OFF ^
  -DD3D11HELPER_INSTALL=ON

cmake --build out/build/install --config Release --parallel
cmake --install out/build/install --config Release --prefix C:\Libraries\D3D11Helper
```

Consumer project:

```cmake
find_package(D3D11Helper CONFIG REQUIRED)
target_link_libraries(MyApp PRIVATE D3D11Helper::D3D11Helper)
```

Configure the consumer with:

```bat
cmake -S . -B out/build -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_PREFIX_PATH=C:\Libraries\D3D11Helper
```

## Package variables

After `find_package(D3D11Helper CONFIG REQUIRED)`, these variables are available:

- `D3D11Helper_VERSION`
- `D3D11Helper_SHADER_DIR`

`D3D11Helper_SHADER_DIR` points to the installed shader asset directory.

## Package smoke test

When D3D11Helper is configured as the top-level project, `D3D11HELPER_ENABLE_PACKAGE_SMOKE_TESTS` defaults to `ON`.

The `PackageSmoke` CTest entry performs:

1. `cmake --install` into a temporary prefix.
2. Configure a minimal consumer project with `find_package(D3D11Helper CONFIG REQUIRED)`.
3. Build that consumer project.

Disable it explicitly with:

```bat
cmake -S . -B out/build -G "Visual Studio 17 2022" -A x64 ^
  -DD3D11HELPER_ENABLE_PACKAGE_SMOKE_TESTS=OFF
```

## Installed files

- `include/D3D11Helper/...`
- static library under the platform library directory
- CMake package files under `lib/cmake/D3D11Helper`
- processing shaders under `share/D3D11Helper/shaders`
