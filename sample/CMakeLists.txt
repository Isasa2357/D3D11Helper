#
# sample/CMakeLists.txt
#
# The sample executables share the same runtime output directory for a given
# configuration, for example:
#   out/build/default/sample/Debug
#
# Therefore shared runtime assets such as shaders and DXC runtime DLLs must not
# be copied by every sample target in POST_BUILD. With parallel MSBuild, multiple
# sample targets can otherwise copy the same directory at the same time and one
# of them can fail with MSB3073. Copy those shared assets once through a single
# target, then make every sample depend on it.
#

set(D3D11HELPER_SAMPLE_RUNTIME_ASSETS_TARGET D3D11HelperSampleRuntimeAssets)

set(D3D11HELPER_SAMPLE_RUNTIME_DIR "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>")

add_custom_target(${D3D11HELPER_SAMPLE_RUNTIME_ASSETS_TARGET} ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "${D3D11HELPER_SAMPLE_RUNTIME_DIR}"
    COMMENT "Copying shared D3D11Helper sample runtime assets"
    VERBATIM
)

if(D3D11HELPER_COPY_DXC_RUNTIME)
    d3d11helper_find_dxc_runtime()

    if(D3D11HELPER_DXCOMPILER_DLL)
        add_custom_command(TARGET ${D3D11HELPER_SAMPLE_RUNTIME_ASSETS_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${D3D11HELPER_DXCOMPILER_DLL}"
                    "${D3D11HELPER_SAMPLE_RUNTIME_DIR}"
            VERBATIM
        )
    endif()

    if(D3D11HELPER_DXIL_DLL)
        add_custom_command(TARGET ${D3D11HELPER_SAMPLE_RUNTIME_ASSETS_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${D3D11HELPER_DXIL_DLL}"
                    "${D3D11HELPER_SAMPLE_RUNTIME_DIR}"
            VERBATIM
        )
    endif()
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../shaders")
    add_custom_command(TARGET ${D3D11HELPER_SAMPLE_RUNTIME_ASSETS_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "${D3D11HELPER_SAMPLE_RUNTIME_DIR}/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/../shaders"
                "${D3D11HELPER_SAMPLE_RUNTIME_DIR}/shaders"
        VERBATIM
    )
endif()

function(d3d11helper_add_sample target_name sample_dir)
    add_executable(${target_name}
        "${sample_dir}/main.cpp"
    )

    target_link_libraries(${target_name}
        PRIVATE
            D3D11Helper::D3D11Helper
    )

    target_compile_definitions(${target_name} PRIVATE
        D3D11HELPER_SAMPLE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /utf-8 /EHsc)
    endif()

    set_target_properties(${target_name} PROPERTIES
        FOLDER "D3D11Helper/Samples"
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${target_name}>"
    )

    add_dependencies(${target_name} ${D3D11HELPER_SAMPLE_RUNTIME_ASSETS_TARGET})

    file(GLOB sample_assets CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/${sample_dir}/*.hlsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/${sample_dir}/*.cso"
    )
    if(sample_assets)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${sample_assets}
                    "$<TARGET_FILE_DIR:${target_name}>"
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
    endif()
endfunction()

d3d11helper_add_sample(D3D11Sample_01_HelloDevice       01_HelloDevice)
d3d11helper_add_sample(D3D11Sample_02_ComputeGrayscale 02_ComputeGrayscale)
d3d11helper_add_sample(D3D11Sample_03_HelloTriangle    03_HelloTriangle)
d3d11helper_add_sample(D3D11Sample_04_BufferCompute    04_BufferCompute)
d3d11helper_add_sample(D3D11Sample_05_DynamicStreaming 05_DynamicStreaming)
d3d11helper_add_sample(D3D11Sample_06_SharedResource   06_SharedResource)
d3d11helper_add_sample(D3D11Sample_07_ProcessingFusedConvertResize 07_ProcessingFusedConvertResize)
d3d11helper_add_sample(D3D11Sample_08_ProcessingP010Rgba16 08_ProcessingP010Rgba16)
d3d11helper_add_sample(D3D11Sample_09_ProcessingBlur 09_ProcessingBlur)
d3d11helper_add_sample(D3D11Sample_10_ProcessingRegionEffect 10_ProcessingRegionEffect)
d3d11helper_add_sample(D3D11Sample_11_ProcessingRegionBlur 11_ProcessingRegionBlur)
d3d11helper_add_sample(D3D11Sample_12_ProcessingColorAdjust 12_ProcessingColorAdjust)
d3d11helper_add_sample(D3D11Sample_13_ProcessingKernelFilter 13_ProcessingKernelFilter)
d3d11helper_add_sample(D3D11Sample_14_ProcessingMask 14_ProcessingMask)
d3d11helper_add_sample(D3D11Sample_15_ProcessingThreshold 15_ProcessingThreshold)
d3d11helper_add_sample(D3D11Sample_16_ProcessingPyramid 16_ProcessingPyramid)
d3d11helper_add_sample(D3D11Sample_17_ProcessingPyramidBlur 17_ProcessingPyramidBlur)
d3d11helper_add_sample(D3D11Sample_18_TextureTransfer 18_TextureTransfer)
d3d11helper_add_sample(D3D11Sample_19_PresentationWindow 19_PresentationWindow)
d3d11helper_add_sample(D3D11Sample_20_ComputeBindingSet 20_ComputeBindingSet)
d3d11helper_add_sample(D3D11Sample_21_Diagnostics 21_Diagnostics)
d3d11helper_add_sample(D3D11Sample_22_CopyResolveMipmap 22_CopyResolveMipmap)
d3d11helper_add_sample(D3D11Sample_23_ViewState 23_ViewState)
