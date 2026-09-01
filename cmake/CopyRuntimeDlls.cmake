if(NOT DEFINED target_dir)
    message(FATAL_ERROR "target_dir is required")
endif()

if(NOT DEFINED source_dir)
    message(FATAL_ERROR "source_dir is required")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${source_dir}" "${target_dir}"
    RESULT_VARIABLE copy_result
)

if(NOT copy_result EQUAL 0)
    message(FATAL_ERROR "Failed to copy runtime DLL directory: ${source_dir}")
endif()
