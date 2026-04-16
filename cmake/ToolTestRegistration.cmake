find_package(Python3 REQUIRED COMPONENTS Interpreter)

function(register_tool_case_tests relative_root expected_family expected_runner generated_case_include generated_registration_file)
    file(GLOB descriptor_paths CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${relative_root}/*/case.toml")
    if(NOT descriptor_paths)
        message(FATAL_ERROR "no tool case descriptors found under: ${CMAKE_CURRENT_SOURCE_DIR}/${relative_root}")
    endif()

    execute_process(
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/tool_case_manifest.py
            --source-root ${CMAKE_CURRENT_SOURCE_DIR}
            --relative-root ${relative_root}
            --expected-family ${expected_family}
            --expected-runner ${expected_runner}
            --emit-case-include ${generated_case_include}
            --emit-ctest-registration ${generated_registration_file}
        RESULT_VARIABLE manifest_result
        OUTPUT_VARIABLE manifest_stdout
        ERROR_VARIABLE manifest_stderr
    )
    if(NOT manifest_result EQUAL 0)
        message(FATAL_ERROR
            "failed to generate grouped tool registration for ${relative_root}\n"
            "stdout:\n${manifest_stdout}\n"
            "stderr:\n${manifest_stderr}")
    endif()

    include(${generated_registration_file})
endfunction()