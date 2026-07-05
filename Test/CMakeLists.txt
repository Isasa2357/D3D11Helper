#
# Test/CMakeLists.txt - CTest registration
#
# Existing D3D11Helper tests are standalone one-file executables with their own
# main(). The newer TEST(Suite, Case)-style tests are built into one harness
# executable with Test/main.cpp. Do not glob every test_*.cpp into one target,
# because that causes multiple main definitions.
#

enable_testing()

set(D3D11HELPER_TEST_DEFAULT_TIMEOUT 60)

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

    if(COMMAND d3d11helper_copy_dxc_runtime)
        d3d11helper_copy_dxc_runtime(${name})
    endif()

    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:${name}>"
        TIMEOUT ${timeout}
    )
endfunction()

# Existing standalone tests. Each file has its own main().
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
add_d3d11_test(test_integration        test_integration.cpp        90)
add_d3d11_test(test_multithread        test_multithread.cpp        90)

# New TEST(Suite, Case)-style tests. These do not define main() and are driven
# by Test/main.cpp. Keep the source list explicit so standalone tests are never
# linked into this executable by mistake.
set(D3D11HELPER_HARNESS_TEST_SOURCES
    main.cpp
    test_FormatUtil.cpp
    test_Helpers.cpp
    test_ShaderCompiler.cpp
)

add_executable(d3d11helper_tests ${D3D11HELPER_HARNESS_TEST_SOURCES})
target_link_libraries(d3d11helper_tests PRIVATE D3D11Helper::D3D11Helper)
target_include_directories(d3d11helper_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

if(MSVC)
    target_compile_options(d3d11helper_tests PRIVATE /utf-8 /W4 /permissive-)
endif()

if(COMMAND d3d11helper_copy_dxc_runtime)
    d3d11helper_copy_dxc_runtime(d3d11helper_tests)
endif()

set(D3D11HELPER_HARNESS_TEST_SUITES
    FormatUtil
    Helpers
    ShaderCompiler
)

foreach(suite IN LISTS D3D11HELPER_HARNESS_TEST_SUITES)
    add_test(NAME ${suite} COMMAND d3d11helper_tests ${suite})
    set_tests_properties(${suite} PROPERTIES
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:d3d11helper_tests>"
        TIMEOUT ${D3D11HELPER_TEST_DEFAULT_TIMEOUT}
    )
endforeach()
