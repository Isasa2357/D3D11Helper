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
                "${D3D11HELPER_TEST_RUNTIME_DIR}/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/../shaders"
                "${D3D11HELPER_TEST_RUNTIME_DIR}/shaders"
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
add_d3d11_test(test_staging            test_staging.cpp)
add_d3d11_test(test_shader_compiler    test_shader_compiler.cpp)
add_d3d11_test(test_compute            test_compute.cpp)
add_d3d11_test(test_graphics           test_graphics.cpp)
add_d3d11_test(test_shared_resource    test_shared_resource.cpp)
add_d3d11_test(test_texture_transfer   test_texture_transfer.cpp 90)
add_d3d11_test(test_integration        test_integration.cpp        90)
add_d3d11_test(test_multithread        test_multithread.cpp        90)

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
