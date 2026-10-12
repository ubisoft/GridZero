# Copyright (c) Ubisoft. All Rights Reserved.
# Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

# =============================================================================
# GridZero CMake API: Plugin & Host Definition
# =============================================================================

function(crg_add_plugin target_name source_file)
    add_library(${target_name} SHARED ${source_file})
    target_link_libraries(${target_name} PRIVATE crg_core)
    
    # Force strict isolation for plugin template statics and inline variables
    set_target_properties(${target_name} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )

    target_compile_definitions(${target_name} PRIVATE CRG_PLUGINS_ENABLED=1)

    if(WIN32)
        target_compile_definitions(${target_name} PRIVATE
            "CRG_EXPORT=__declspec(dllexport)"
            "CRG_SHARED_LIB_PREFIX=\"\""
            "CRG_SHARED_LIB_EXTENSION=\".dll\""
        )
    elseif(APPLE)
        target_compile_definitions(${target_name} PRIVATE
            "CRG_EXPORT=__attribute__((visibility(\"default\")))"
            "CRG_SHARED_LIB_PREFIX=\"lib\""
            "CRG_SHARED_LIB_EXTENSION=\".dylib\""
        )
    else()
        target_compile_definitions(${target_name} PRIVATE
            "CRG_EXPORT=__attribute__((visibility(\"default\")))"
            "CRG_SHARED_LIB_PREFIX=\"lib\""
            "CRG_SHARED_LIB_EXTENSION=\".so\""
        )
        target_link_libraries(${target_name} PRIVATE dl)
    endif()
endfunction()

function(crg_add_host target_name source_file main_runner)
    add_executable(${target_name} ${source_file} ${main_runner})
    target_link_libraries(${target_name} PRIVATE crg_core)

    # Force strict isolation for plugin template statics and inline variables
    set_target_properties(${target_name} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )

    target_compile_definitions(${target_name} PRIVATE CRG_PLUGINS_ENABLED=1)

    if(WIN32)
        target_compile_definitions(${target_name} PRIVATE
            "CRG_EXPORT="
            "CRG_SHARED_LIB_PREFIX=\"\""
            "CRG_SHARED_LIB_EXTENSION=\".dll\""
        )
    elseif(APPLE)
        target_compile_definitions(${target_name} PRIVATE
            "CRG_EXPORT=__attribute__((visibility(\"default\")))"
            "CRG_SHARED_LIB_PREFIX=\"lib\""
            "CRG_SHARED_LIB_EXTENSION=\".dylib\""
        )
    else()
        target_compile_definitions(${target_name} PRIVATE
            "CRG_EXPORT=__attribute__((visibility(\"default\")))"
            "CRG_SHARED_LIB_PREFIX=\"lib\""
            "CRG_SHARED_LIB_EXTENSION=\".so\""
        )
        target_link_libraries(${target_name} PRIVATE dl)
    endif()
endfunction()