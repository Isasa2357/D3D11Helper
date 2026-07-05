#
# Test/CMakeLists.txt - CTest 向け分割テスト
#
enable_testing()

# テスト共通マクロ
# 万一 GPU 同期の不具合（ドライバ差異、WARP の制約など）でテストがハングしても
# テストスイート全体を止めないよう、既定のタイムアウトを設定する。
# 個別に長めのタイムアウトが必要なテストは第3引数で上書きできる。
set(D3D11HELPER_TEST_DEFAULT_TIMEOUT 60)

function(add_d3d11_test name source)
    set(timeout ${D3D11HELPER_TEST_DEFAULT_TIMEOUT})
    if(ARGC GREATER 2)
        set(timeout ${ARGV2})
    endif()

    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE D3D11Helper::D3D11Helper)
    # D3D11Helper::D3D11Helper が PUBLIC で公開している
    # include/D3D11Helper (= "D3D11Core/..." "D3D11Framework/..." で参照できる)
    # に加えて、テスト共通ヘッダ TestUtil.hpp を見つけられるようにする。
    target_include_directories(${name} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:${name}>"
        TIMEOUT ${timeout}
    )
endfunction()

# --- 12 テスト ---
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
