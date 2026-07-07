cmake_minimum_required(VERSION 3.20)

project(D3D11Helper
    VERSION 1.10.0
    LANGUAGES CXX
)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(D3D11HELPER_INSTALL_DEFAULT ON)
    set(D3D11HELPER_PACKAGE_SMOKE_DEFAULT ON)
else()
    set(D3D11HELPER_INSTALL_DEFAULT OFF)
    set(D3D11HELPER_PACKAGE_SMOKE_DEFAULT OFF)
endif()

option(D3D11HELPER_BUILD_SAMPLES "Build D3D11Helper samples" ON)
option(D3D11HELPER_BUILD_TESTS "Build D3D11Helper tests" ON)
option(D3D11HELPER_ENABLE_WARNINGS "Enable compiler warnings for D3D11Helper" ON)
option(D3D11HELPER_INSTALL "Enable D3D11Helper install rules" ${D3D11HELPER_INSTALL_DEFAULT})
option(D3D11HELPER_ENABLE_PACKAGE_SMOKE_TESTS "Enable install/find_package smoke tests" ${D3D11HELPER_PACKAGE_SMOKE_DEFAULT})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    message(WARNING "D3D11Helper is a Direct3D 11 helper library and can only be built on Windows. No targets will be generated on this platform.")
    return()
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(D3D11HELPER_ROOT_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(D3D11HELPER_INCLUDE_DIR "${D3D11HELPER_ROOT_INCLUDE_DIR}/D3D11Helper")
set(D3D11HELPER_FOUNDATION_INCLUDE_DIR "${D3D11HELPER_INCLUDE_DIR}/D3D11Foundation")
set(D3D11HELPER_CORE_INCLUDE_DIR "${D3D11HELPER_INCLUDE_DIR}/D3D11Core")
set(D3D11HELPER_GPU_INCLUDE_DIR "${D3D11HELPER_INCLUDE_DIR}/D3D11Gpu")
set(D3D11HELPER_FRAMEWORK_INCLUDE_DIR "${D3D11HELPER_INCLUDE_DIR}/D3D11Framework")
set(D3D11HELPER_PRESENTATION_INCLUDE_DIR "${D3D11HELPER_INCLUDE_DIR}/D3D11Presentation")
set(D3D11HELPER_PROCESSING_INCLUDE_DIR "${D3D11HELPER_INCLUDE_DIR}/D3D11Processing")
set(D3D11HELPER_INTEROP_INCLUDE_DIR "${D3D11HELPER_INCLUDE_DIR}/D3D11Interop")
set(D3D11HELPER_DIAGNOSTICS_INCLUDE_DIR "${D3D11HELPER_INCLUDE_DIR}/D3D11Diagnostics")

set(D3D11HELPER_SOURCES
    src/ThrowIfFailed.cpp
    src/DxgiUtil.cpp
    src/DxgiAdapterSelector.cpp
    src/D3D11Debug.cpp
    src/D3D11DeviceLost.cpp
    src/D3D11InfoQueue.cpp
    src/D3D11GpuTimer.cpp
    src/D3D11GpuProfiler.cpp
    src/D3D11DeviceContext.cpp
    src/D3D11DeferredContext.cpp
    src/D3D11Core.cpp
    src/D3D11Fence.cpp
    src/D3D11FenceSupport.cpp
    src/D3D11FormatUtil.cpp
    src/D3D11SharedHandle.cpp
    src/D3D11SharedResource.cpp
    src/D3D11SharedTexture.cpp
    src/D3D11KeyedMutex.cpp
    src/D3D11Helpers.cpp
    src/D3D11SharedTextureSync.cpp
    src/D3D11StagingBuffer.cpp
    src/D3D11CpuImage.cpp
    src/D3D11TextureTransfer.cpp
    src/D3D11Copy.cpp
    src/D3D11Resolve.cpp
    src/D3D11Mipmap.cpp
    src/D3D11View.cpp
    src/D3D11State.cpp
    src/D3D11BindingSet.cpp
    src/D3D11ComputePipeline.cpp
    src/D3D11GraphicsPipeline.cpp
    src/D3D11ShaderCompiler.cpp
    src/D3D11ShaderReflection.cpp
    src/D3D11SwapChainHelper.cpp
    src/D3D11RenderTarget.cpp
    src/D3D11SwapChain.cpp
    src/D3D11ProcessingTypes.cpp
    src/D3D11ProcessingContext.cpp
    src/D3D11ProcessingShaderCache.cpp
    src/D3D11TextureViews.cpp
    src/D3D11FormatConverter.cpp
    src/D3D11Resize.cpp
    src/D3D11Remap.cpp
    src/D3D11Composite.cpp
    src/D3D11Blur.cpp
    src/D3D11RegionEffect.cpp
    src/D3D11RegionBlur.cpp
    src/D3D11ColorAdjust.cpp
    src/D3D11KernelFilter.cpp
    src/D3D11MaskProcessor.cpp
    src/D3D11ThresholdProcessor.cpp
    src/D3D11PyramidProcessor.cpp
    src/D3D11PyramidBlur.cpp
    src/D3D11PyramidRegionBlur.cpp
    src/D3D11FusedPipeline.cpp
)

# v1.1.0+ module layout:
#   D3D11Foundation  : DirectX/DXGI-only utilities
#   D3D11Core        : device / context facade
#   D3D11Gpu         : resource / view / transfer / pipeline helpers
#   D3D11Presentation: swapchain / backbuffer helpers
#   D3D11Processing  : GPU image processing
#   D3D11Interop     : shared handles / D3D11-D3D12 interop
#   D3D11Diagnostics : debug layer / InfoQueue / live object helpers
#
# D3D11Framework and several D3D11Core headers remain in this list as
# compatibility wrappers for v1.x users.  New code should prefer the module
# headers above.

set(D3D11HELPER_PUBLIC_HEADERS
    include/D3D11Helper/D3D11Foundation/D3D11Common.hpp
    include/D3D11Helper/D3D11Foundation/D3D11Foundation.hpp
    include/D3D11Helper/D3D11Foundation/ThrowIfFailed.hpp
    include/D3D11Helper/D3D11Foundation/DxgiAdapterSelector.hpp
    include/D3D11Helper/D3D11Foundation/DxgiUtil.hpp
    include/D3D11Helper/D3D11Foundation/D3D11FormatUtil.hpp

    include/D3D11Helper/D3D11Core/D3D11Common.hpp
    include/D3D11Helper/D3D11Core/D3D11Core.hpp
    include/D3D11Helper/D3D11Core/D3D11CoreConfig.hpp
    include/D3D11Helper/D3D11Core/D3D11Debug.hpp
    include/D3D11Helper/D3D11Core/D3D11DeviceContext.hpp
    include/D3D11Helper/D3D11Core/D3D11DeferredContext.hpp
    include/D3D11Helper/D3D11Core/D3D11Fence.hpp
    include/D3D11Helper/D3D11Core/D3D11FormatUtil.hpp
    include/D3D11Helper/D3D11Core/D3D11SharedResource.hpp
    include/D3D11Helper/D3D11Core/DxgiAdapterSelector.hpp
    include/D3D11Helper/D3D11Core/DxgiUtil.hpp
    include/D3D11Helper/D3D11Core/ThrowIfFailed.hpp

    include/D3D11Helper/D3D11Gpu/D3D11Gpu.hpp
    include/D3D11Helper/D3D11Gpu/D3D11CpuImage.hpp
    include/D3D11Helper/D3D11Gpu/D3D11TextureTransfer.hpp
    include/D3D11Helper/D3D11Gpu/D3D11Copy.hpp
    include/D3D11Helper/D3D11Gpu/D3D11Resolve.hpp
    include/D3D11Helper/D3D11Gpu/D3D11Mipmap.hpp
    include/D3D11Helper/D3D11Gpu/D3D11View.hpp
    include/D3D11Helper/D3D11Gpu/D3D11State.hpp
    include/D3D11Helper/D3D11Gpu/D3D11BindingSet.hpp
    include/D3D11Helper/D3D11Gpu/D3D11ComputePipeline.hpp
    include/D3D11Helper/D3D11Gpu/D3D11GraphicsPipeline.hpp
    include/D3D11Helper/D3D11Gpu/D3D11Helpers.hpp
    include/D3D11Helper/D3D11Gpu/D3D11Resource.hpp
    include/D3D11Helper/D3D11Gpu/D3D11ShaderCompiler.hpp
    include/D3D11Helper/D3D11Gpu/D3D11ShaderReflection.hpp
    include/D3D11Helper/D3D11Gpu/D3D11StagingBuffer.hpp

    include/D3D11Helper/D3D11Framework/D3D11ComputePipeline.hpp
    include/D3D11Helper/D3D11Framework/D3D11Framework.hpp
    include/D3D11Helper/D3D11Framework/D3D11GraphicsPipeline.hpp
    include/D3D11Helper/D3D11Framework/D3D11Helpers.hpp
    include/D3D11Helper/D3D11Framework/D3D11Resource.hpp
    include/D3D11Helper/D3D11Framework/D3D11ShaderCompiler.hpp
    include/D3D11Helper/D3D11Framework/D3D11StagingBuffer.hpp
    include/D3D11Helper/D3D11Framework/D3D11SwapChainHelper.hpp

    include/D3D11Helper/D3D11Presentation/D3D11Presentation.hpp
    include/D3D11Helper/D3D11Presentation/D3D11SwapChainHelper.hpp
    include/D3D11Helper/D3D11Presentation/D3D11RenderTarget.hpp
    include/D3D11Helper/D3D11Presentation/D3D11SwapChain.hpp

    include/D3D11Helper/D3D11Interop/D3D11Interop.hpp
    include/D3D11Helper/D3D11Interop/D3D11Fence.hpp
    include/D3D11Helper/D3D11Interop/D3D11FenceSupport.hpp
    include/D3D11Helper/D3D11Interop/D3D11SharedHandle.hpp
    include/D3D11Helper/D3D11Interop/D3D11SharedResource.hpp
    include/D3D11Helper/D3D11Interop/D3D11SharedTexture.hpp
    include/D3D11Helper/D3D11Interop/D3D11KeyedMutex.hpp

    include/D3D11Helper/D3D11Diagnostics/D3D11Diagnostics.hpp
    include/D3D11Helper/D3D11Diagnostics/D3D11Debug.hpp
    include/D3D11Helper/D3D11Diagnostics/D3D11DeviceLost.hpp
    include/D3D11Helper/D3D11Diagnostics/D3D11InfoQueue.hpp
    include/D3D11Helper/D3D11Diagnostics/D3D11GpuTimer.hpp
    include/D3D11Helper/D3D11Diagnostics/D3D11GpuProfiler.hpp

    include/D3D11Helper/D3D11Processing/D3D11Processing.hpp
    include/D3D11Helper/D3D11Processing/D3D11ProcessingTypes.hpp
    include/D3D11Helper/D3D11Processing/D3D11ProcessingContext.hpp
    include/D3D11Helper/D3D11Processing/D3D11ProcessingShaderCache.hpp
    include/D3D11Helper/D3D11Processing/D3D11TextureViews.hpp
    include/D3D11Helper/D3D11Processing/D3D11FormatConverter.hpp
    include/D3D11Helper/D3D11Processing/D3D11Resize.hpp
    include/D3D11Helper/D3D11Processing/D3D11Remap.hpp
    include/D3D11Helper/D3D11Processing/D3D11Composite.hpp
    include/D3D11Helper/D3D11Processing/D3D11Blur.hpp
    include/D3D11Helper/D3D11Processing/D3D11RegionEffect.hpp
    include/D3D11Helper/D3D11Processing/D3D11RegionBlur.hpp
    include/D3D11Helper/D3D11Processing/D3D11ColorAdjust.hpp
    include/D3D11Helper/D3D11Processing/D3D11KernelFilter.hpp
    include/D3D11Helper/D3D11Processing/D3D11MaskProcessor.hpp
    include/D3D11Helper/D3D11Processing/D3D11ThresholdProcessor.hpp
    include/D3D11Helper/D3D11Processing/D3D11PyramidProcessor.hpp
    include/D3D11Helper/D3D11Processing/D3D11PyramidBlur.hpp
    include/D3D11Helper/D3D11Processing/D3D11PyramidRegionBlur.hpp
    include/D3D11Helper/D3D11Processing/D3D11FusedPipeline.hpp
)

add_library(D3D11Helper STATIC ${D3D11HELPER_SOURCES} ${D3D11HELPER_PUBLIC_HEADERS})
add_library(D3D11Helper::D3D11Helper ALIAS D3D11Helper)

target_include_directories(D3D11Helper
    PUBLIC
        $<BUILD_INTERFACE:${D3D11HELPER_ROOT_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_definitions(D3D11Helper
    PUBLIC
        WIN32_LEAN_AND_MEAN
        NOMINMAX
)

target_link_libraries(D3D11Helper
    PUBLIC
        d3d11
        dxgi
        dxguid
        d3dcompiler
        dxcompiler
        user32
)

if(MSVC)
    target_compile_options(D3D11Helper PRIVATE /utf-8 /EHsc)
    if(D3D11HELPER_ENABLE_WARNINGS)
        target_compile_options(D3D11Helper PRIVATE /W4 /permissive-)
    endif()
else()
    if(D3D11HELPER_ENABLE_WARNINGS)
        target_compile_options(D3D11Helper PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "Source" FILES ${D3D11HELPER_SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include" PREFIX "Header" FILES ${D3D11HELPER_PUBLIC_HEADERS})

include(cmake/D3D11HelperDxcRuntime.cmake)

if(D3D11HELPER_INSTALL)
    set(D3D11HELPER_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/D3D11Helper")

    install(TARGETS D3D11Helper
        EXPORT D3D11HelperTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )

    install(DIRECTORY "${D3D11HELPER_ROOT_INCLUDE_DIR}/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )

    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/shaders/"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/D3D11Helper/shaders"
        OPTIONAL
    )

    install(EXPORT D3D11HelperTargets
        FILE D3D11HelperTargets.cmake
        NAMESPACE D3D11Helper::
        DESTINATION "${D3D11HELPER_INSTALL_CMAKEDIR}"
    )

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/D3D11HelperConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/D3D11HelperConfig.cmake"
        INSTALL_DESTINATION "${D3D11HELPER_INSTALL_CMAKEDIR}"
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/D3D11HelperConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/D3D11HelperConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/D3D11HelperConfigVersion.cmake"
        DESTINATION "${D3D11HELPER_INSTALL_CMAKEDIR}"
    )
endif()

if(D3D11HELPER_BUILD_SAMPLES)
    add_subdirectory(sample)
endif()

if(D3D11HELPER_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(Test)
endif()
