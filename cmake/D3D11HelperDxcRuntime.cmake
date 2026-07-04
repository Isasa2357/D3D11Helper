# Helpers for locating/copying the DirectX Shader Compiler runtime.
# dxcompiler.dll is required at runtime when DxcCreateInstance is used.

set(D3D11HELPER_DXC_RUNTIME_DIR "" CACHE PATH "Directory that contains dxcompiler.dll and dxil.dll")
option(D3D11HELPER_COPY_DXC_RUNTIME "Copy dxcompiler.dll and dxil.dll next to built executables" ON)

function(d3d11helper_find_dxc_runtime)
    if(DEFINED D3D11HELPER_DXCOMPILER_DLL AND EXISTS "${D3D11HELPER_DXCOMPILER_DLL}")
        return()
    endif()

    # NuGet's Microsoft.Direct3D.DXC package currently contains both x64 and x86
    # runtime folders. Select the one that matches the CMake generator platform.
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_dxc_arch "x64")
    else()
        set(_dxc_arch "x86")
    endif()

    set(_dxc_candidate_dirs)

    if(D3D11HELPER_DXC_RUNTIME_DIR)
        file(TO_CMAKE_PATH "${D3D11HELPER_DXC_RUNTIME_DIR}" _dxc_runtime_dir)
        list(APPEND _dxc_candidate_dirs "${_dxc_runtime_dir}")
    endif()

    # Common locations when the NuGet package was restored locally.
    # Do not use CONFIGURE_DEPENDS here. When USERPROFILE contains backslashes,
    # CMake can emit an invalid VerifyGlobs.cmake entry such as "C:\Users...".
    file(GLOB _local_package_dirs
        "${PROJECT_SOURCE_DIR}/packages/Microsoft.Direct3D.DXC*/bin/${_dxc_arch}"
        "${PROJECT_SOURCE_DIR}/packages/microsoft.direct3d.dxc*/bin/${_dxc_arch}"
        "${PROJECT_SOURCE_DIR}/packages/Microsoft.Direct3D.DXC*/build/native/bin/${_dxc_arch}"
        "${PROJECT_SOURCE_DIR}/packages/microsoft.direct3d.dxc*/build/native/bin/${_dxc_arch}"
        "${CMAKE_BINARY_DIR}/packages/Microsoft.Direct3D.DXC*/bin/${_dxc_arch}"
        "${CMAKE_BINARY_DIR}/packages/microsoft.direct3d.dxc*/bin/${_dxc_arch}"
        "${CMAKE_BINARY_DIR}/packages/Microsoft.Direct3D.DXC*/build/native/bin/${_dxc_arch}"
        "${CMAKE_BINARY_DIR}/packages/microsoft.direct3d.dxc*/build/native/bin/${_dxc_arch}"
    )
    list(APPEND _dxc_candidate_dirs ${_local_package_dirs})

    # Common location used by the global NuGet cache.
    if(DEFINED ENV{USERPROFILE})
        file(TO_CMAKE_PATH "$ENV{USERPROFILE}" _user_profile)
        file(GLOB _global_package_dirs
            "${_user_profile}/.nuget/packages/microsoft.direct3d.dxc/*/bin/${_dxc_arch}"
            "${_user_profile}/.nuget/packages/microsoft.direct3d.dxc/*/build/native/bin/${_dxc_arch}"
        )
        list(APPEND _dxc_candidate_dirs ${_global_package_dirs})
    endif()

    list(REMOVE_DUPLICATES _dxc_candidate_dirs)

    find_file(D3D11HELPER_DXCOMPILER_DLL
        NAMES dxcompiler.dll
        PATHS ${_dxc_candidate_dirs}
        NO_DEFAULT_PATH
    )

    find_file(D3D11HELPER_DXIL_DLL
        NAMES dxil.dll
        PATHS ${_dxc_candidate_dirs}
        NO_DEFAULT_PATH
    )

    if(D3D11HELPER_DXCOMPILER_DLL)
        message(STATUS "D3D11Helper: found dxcompiler.dll: ${D3D11HELPER_DXCOMPILER_DLL}")
        if(D3D11HELPER_DXIL_DLL)
            message(STATUS "D3D11Helper: found dxil.dll: ${D3D11HELPER_DXIL_DLL}")
        endif()
    else()
        message(WARNING
            "D3D11Helper: dxcompiler.dll was not found. "
            "DXC tests/samples may fail at runtime. "
            "Restore Microsoft.Direct3D.DXC with NuGet, or pass "
            "-DD3D11HELPER_DXC_RUNTIME_DIR=<dir containing dxcompiler.dll>."
        )
    endif()
endfunction()

function(d3d11helper_copy_dxc_runtime target_name)
    if(NOT D3D11HELPER_COPY_DXC_RUNTIME)
        return()
    endif()

    d3d11helper_find_dxc_runtime()

    if(D3D11HELPER_DXCOMPILER_DLL)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${D3D11HELPER_DXCOMPILER_DLL}"
                    "$<TARGET_FILE_DIR:${target_name}>"
            VERBATIM
        )
    endif()

    if(D3D11HELPER_DXIL_DLL)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${D3D11HELPER_DXIL_DLL}"
                    "$<TARGET_FILE_DIR:${target_name}>"
            VERBATIM
        )
    endif()
endfunction()
