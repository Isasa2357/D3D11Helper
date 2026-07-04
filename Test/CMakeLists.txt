add_executable(D3D11HelperTest
    "${CMAKE_CURRENT_SOURCE_DIR}/D3D11HelperTest.cpp"
)

target_link_libraries(D3D11HelperTest
    PRIVATE
        D3D11Helper::D3D11Helper
)

# D3D11HelperTest.cpp is intentionally kept outside src/ so that
# projects can copy/add src/ as library implementation files only.
# The test still uses legacy relative includes such as
# "../D3D11Core/D3D11Common.hpp", so these include roots keep it
# buildable without rewriting the test source.
target_include_directories(D3D11HelperTest
    PRIVATE
        "${PROJECT_SOURCE_DIR}/include/D3D11Helper/D3D11Core"
        "${PROJECT_SOURCE_DIR}/include/D3D11Helper/D3D11Framework"
)


set_target_properties(D3D11HelperTest PROPERTIES
    FOLDER "D3D11Helper/Tests"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:D3D11HelperTest>"
)

d3d11helper_copy_dxc_runtime(D3D11HelperTest)

add_test(NAME D3D11HelperTest COMMAND D3D11HelperTest)
set_tests_properties(D3D11HelperTest PROPERTIES
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:D3D11HelperTest>"
)
