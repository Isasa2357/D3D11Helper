cmake_minimum_required(VERSION 3.20)

project(D3D11HelperPackageSmoke LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(D3D11Helper CONFIG REQUIRED)

add_executable(d3d11helper_package_smoke main.cpp)
target_link_libraries(d3d11helper_package_smoke PRIVATE D3D11Helper::D3D11Helper)

if(MSVC)
    target_compile_options(d3d11helper_package_smoke PRIVATE /utf-8 /EHsc)
endif()
