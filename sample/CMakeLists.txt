function(d3d11helper_add_sample target_name sample_dir)
    add_executable(${target_name}
        "${sample_dir}/main.cpp"
    )

    target_link_libraries(${target_name}
        PRIVATE
            D3D11Helper::D3D11Helper
    )


    set_target_properties(${target_name} PROPERTIES
        FOLDER "D3D11Helper/Samples"
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${target_name}>"
    )

    d3d11helper_copy_dxc_runtime(${target_name})

    # Keep loose sample assets next to the executable when a sample directory has them.
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
        )
    endif()
endfunction()

d3d11helper_add_sample(D3D11Sample_01_HelloDevice       01_HelloDevice)
d3d11helper_add_sample(D3D11Sample_02_ComputeGrayscale 02_ComputeGrayscale)
d3d11helper_add_sample(D3D11Sample_03_HelloTriangle    03_HelloTriangle)
d3d11helper_add_sample(D3D11Sample_04_BufferCompute    04_BufferCompute)
d3d11helper_add_sample(D3D11Sample_05_DynamicStreaming 05_DynamicStreaming)
d3d11helper_add_sample(D3D11Sample_06_SharedResource   06_SharedResource)
