cmake_minimum_required(VERSION 3.20)

project(D3D11HelperPackageSmoke LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(D3D11Helper CONFIG REQUIRED)

if(NOT D3D11Helper_SHADER_DIR MATCHES "D3D11Helper")
    message(FATAL_ERROR "D3D11Helper_SHADER_DIR must be namespaced: ${D3D11Helper_SHADER_DIR}")
endif()

if(NOT EXISTS "${D3D11Helper_PROCESSING_SHADER_DIR}")
    message(FATAL_ERROR "D3D11Helper_PROCESSING_SHADER_DIR does not exist: ${D3D11Helper_PROCESSING_SHADER_DIR}")
endif()

add_executable(d3d11helper_package_smoke main.cpp)
target_link_libraries(d3d11helper_package_smoke PRIVATE D3D11Helper::D3D11Helper)

if(MSVC)
    target_compile_options(d3d11helper_package_smoke PRIVATE /utf-8 /EHsc)
endif()
