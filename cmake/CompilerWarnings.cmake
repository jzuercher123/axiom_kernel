function(axiom_set_project_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} INTERFACE
                /W4
                /permissive-
                /Zc:__cplusplus
        )
    else()
        target_compile_options(${target_name} INTERFACE
                -Wall
                -Wextra
                -Wpedantic
                -Wshadow
                -Wconversion
                -Wsign-conversion
        )
    endif()
endfunction()