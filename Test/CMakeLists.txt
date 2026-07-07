#
# Test/CMakeLists.txt - CTest registration
#
enable_testing()

set(D3D11HELPER_TEST_DEFAULT_TIMEOUT 60)
set(D3D11HELPER_TEST_RUNTIME_ASSETS_TARGET D3D11HelperTestRuntimeAssets)
set(D3D11HELPER_TEST_RUNTIME_DIR "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>")

add_custom_target(${D3D11HELPER_TEST_RUNTIME_ASSETS_TARGET} ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "${D3D11HELPER_TEST_RUNTIME_DIR}"
    COMMENT "Copying shared D3D11Helper test runtime assets"
    VERBATIM
)

if(D3D11HELPER_COPY_DXC_RUNTIME)
    d3d11helper_find_dxc_runtime()

    if(D3D11HELPER_DXCOMPILER_DLL)
        add_custom_command(TARGET ${D3D11HELPER_TEST_RUNTIME_ASSETS_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${D3D11HELPER_DXCOMPILER_DLL}"
                    "${D3D11HELPER_TEST_RUNTIME_DIR}"
            VERBATIM
        )
    endif()

    if(D3D11HELPER_DXIL_DLL)
        add_custom_command(TARGET ${D3D11HELPER_TEST_RUNTIME_ASSETS_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${D3D11HELPER_DXIL_DLL}"
                    "${D3D11HELPER_TEST_RUNTIME_DIR}"
            VERBATIM
        )
    endif()
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../shaders")
    add_custom_command(TARGET ${D3D11HELPER_TEST_RUNTIME_ASSETS_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "${D3D11HELPER_TEST_RUNTIME_DIR}/D3D11Helper/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/../shaders"
                "${D3D11HELPER_TEST_RUNTIME_DIR}/D3D11Helper/shaders"
        VERBATIM
    )
endif()

function(add_d3d11_test name source)
    set(timeout ${D3D11HELPER_TEST_DEFAULT_TIMEOUT})
    if(ARGC GREATER 2)
        set(timeout ${ARGV2})
    endif()

    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE D3D11Helper::D3D11Helper)
    target_include_directories(${name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

    if(MSVC)
        target_compile_options(${name} PRIVATE /utf-8 /W4 /permissive-)
    endif()

    add_dependencies(${name} ${D3D11HELPER_TEST_RUNTIME_ASSETS_TARGET})

    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:${name}>"
        TIMEOUT ${timeout}
    )
endfunction()

add_d3d11_test(test_module_headers     test_module_headers.cpp)
add_d3d11_test(test_utilities          test_utilities.cpp)
add_d3d11_test(test_core               test_core.cpp)
add_d3d11_test(test_buffer             test_buffer.cpp)
add_d3d11_test(test_texture            test_texture.cpp)
add_d3d11_test(test_view               test_view.cpp)
add_d3d11_test(test_binding_set        test_binding_set.cpp)
add_d3d11_test(test_staging            test_staging.cpp)
add_d3d11_test(test_shader_compiler    test_shader_compiler.cpp)
add_d3d11_test(test_compute            test_compute.cpp)
add_d3d11_test(test_graphics           test_graphics.cpp)
add_d3d11_test(test_shared_resource    test_shared_resource.cpp)
add_d3d11_test(test_interop_shared     test_interop_shared.cpp 90)
add_d3d11_test(test_interop_keyed_mutex test_interop_keyed_mutex.cpp 90)
add_d3d11_test(test_interop_fence       test_interop_fence.cpp       90)
add_d3d11_test(test_interop_edge_cases  test_interop_edge_cases.cpp  90)
add_d3d11_test(test_gpu_edge_cases      test_gpu_edge_cases.cpp      120)
add_d3d11_test(test_presentation_diagnostics_shader_edges test_presentation_diagnostics_shader_edges.cpp 120)
add_d3d11_test(test_processing_golden  test_processing_golden.cpp  120)
add_d3d11_test(test_processing_effect_golden test_processing_effect_golden.cpp 120)
add_d3d11_test(test_processing_mask_composition_golden test_processing_mask_composition_golden.cpp 120)
add_d3d11_test(test_processing_pyramid_golden test_processing_pyramid_golden.cpp 120)
add_d3d11_test(test_texture_transfer   test_texture_transfer.cpp 90)
add_d3d11_test(test_copy               test_copy.cpp             90)
add_d3d11_test(test_view_state         test_view_state.cpp       90)
add_d3d11_test(test_render_target      test_render_target.cpp)
add_d3d11_test(test_swap_chain         test_swap_chain.cpp         90)
add_d3d11_test(test_integration        test_integration.cpp        90)
add_d3d11_test(test_multithread        test_multithread.cpp        90)
add_d3d11_test(test_diagnostics        test_diagnostics.cpp        90)

option(D3D11HELPER_ENABLE_D3D12_INTEROP_TESTS "Build optional D3D11/D3D12 interop E2E tests" OFF)
set(D3D11HELPER_D3D12HELPER_GIT_REPOSITORY "https://github.com/Isasa2357/D3D12Helper.git" CACHE STRING "D3D12Helper git repository for optional interop tests")
set(D3D11HELPER_D3D12HELPER_GIT_TAG "main" CACHE STRING "D3D12Helper git tag, branch, or commit for optional interop tests")

if(D3D11HELPER_ENABLE_D3D12_INTEROP_TESTS)
    include(FetchContent)

    set(D3D12HELPER_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
    set(D3D12HELPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        D3D12Helper
        GIT_REPOSITORY "${D3D11HELPER_D3D12HELPER_GIT_REPOSITORY}"
        GIT_TAG        "${D3D11HELPER_D3D12HELPER_GIT_TAG}"
    )
    FetchContent_MakeAvailable(D3D12Helper)

    add_d3d11_test(test_interop_d3d12 test_interop_d3d12.cpp 120)
    target_link_libraries(test_interop_d3d12 PRIVATE D3D12Helper::D3D12Helper)
endif()

set(D3D11HELPER_HARNESS_TEST_SOURCES
    main.cpp
    test_FormatUtil.cpp
    test_Helpers.cpp
    test_ShaderCompiler.cpp
    test_Processing.cpp
    test_ProcessingBlur.cpp
    test_ProcessingRegionEffect.cpp
    test_ProcessingRegionBlur.cpp
    test_ProcessingColorAdjust.cpp
    test_ProcessingKernelFilter.cpp
    test_ProcessingMask.cpp
    test_ProcessingThreshold.cpp
    test_ProcessingPyramid.cpp
    test_ProcessingPyramidBlur.cpp
)

add_executable(d3d11helper_tests ${D3D11HELPER_HARNESS_TEST_SOURCES})
target_link_libraries(d3d11helper_tests PRIVATE D3D11Helper::D3D11Helper)
target_include_directories(d3d11helper_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(d3d11helper_tests PRIVATE
    D3D11HELPER_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

if(MSVC)
    target_compile_options(d3d11helper_tests PRIVATE /utf-8 /W4 /permissive-)
endif()

add_dependencies(d3d11helper_tests ${D3D11HELPER_TEST_RUNTIME_ASSETS_TARGET})

set(D3D11HELPER_HARNESS_TEST_SUITES
    FormatUtil
    Helpers
    ShaderCompiler
    Processing
    ProcessingBlur
    ProcessingRegionEffect
    ProcessingRegionBlur
    ProcessingColorAdjust
    ProcessingKernelFilter
    ProcessingMask
    ProcessingThreshold
    ProcessingPyramid
    ProcessingPyramidBlur
)

foreach(suite IN LISTS D3D11HELPER_HARNESS_TEST_SUITES)
    add_test(NAME ${suite} COMMAND d3d11helper_tests ${suite})
    set_tests_properties(${suite} PROPERTIES
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:d3d11helper_tests>"
        TIMEOUT ${D3D11HELPER_TEST_DEFAULT_TIMEOUT}
    )
endforeach()

if(D3D11HELPER_ENABLE_PACKAGE_SMOKE_TESTS)
    if(NOT D3D11HELPER_INSTALL)
        message(WARNING "D3D11 package smoke tests require D3D11HELPER_INSTALL=ON and will be skipped.")
    else()
        add_test(NAME PackageSmoke COMMAND
            ${CMAKE_COMMAND}
                -DD3D11HELPER_PACKAGE_SMOKE_ROOT_BUILD_DIR=${CMAKE_BINARY_DIR}
                -DD3D11HELPER_PACKAGE_SMOKE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/package_smoke
                -DD3D11HELPER_PACKAGE_SMOKE_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/package_smoke_build/$<CONFIG>
                -DD3D11HELPER_PACKAGE_SMOKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/package_smoke_install/$<CONFIG>
                -DD3D11HELPER_PACKAGE_SMOKE_CONFIG=$<CONFIG>
                -DD3D11HELPER_PACKAGE_SMOKE_GENERATOR=${CMAKE_GENERATOR}
                -DD3D11HELPER_PACKAGE_SMOKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/PackageSmokeTest.cmake
        )
        set_tests_properties(PackageSmoke PROPERTIES
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            TIMEOUT 180
        )
    endif()
endif()
