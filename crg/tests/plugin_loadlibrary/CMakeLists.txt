# stage_plugin_loadlibrary — Manual-load plugin stage (CppCon Acte 1 demo)
#
# Two targets:
#   crg_greeter_plugin             — shared library (.dll/.so) loaded at runtime
#   Run_PluginLoadLibrary_Tests    — host executable; uses its own LoadLibrary call
#
# Neither target links any GridZero .cpp — crg_core is header-only.

cmake_minimum_required(VERSION 3.15)

# ── Plugin DLL ────────────────────────────────────────────────────────────────
add_library(crg_greeter_plugin SHARED plugin_greeter.cpp)

target_link_libraries(crg_greeter_plugin PRIVATE crg_core)

target_compile_definitions(crg_greeter_plugin PRIVATE
    CRG_PLUGINS_ENABLED=1)

if(WIN32)
    target_compile_definitions(crg_greeter_plugin PRIVATE
        "CRG_EXPORT=__declspec(dllexport)"
        "CRG_SHARED_LIB_PREFIX=\"\""
        "CRG_SHARED_LIB_EXTENSION=\".dll\"")
elseif(APPLE)
    target_compile_definitions(crg_greeter_plugin PRIVATE
        "CRG_EXPORT=__attribute__((visibility(\"default\")))"
        "CRG_SHARED_LIB_PREFIX=\"lib\""
        "CRG_SHARED_LIB_EXTENSION=\".dylib\"")
    set_target_properties(crg_greeter_plugin PROPERTIES POSITION_INDEPENDENT_CODE ON)
else()
    target_compile_definitions(crg_greeter_plugin PRIVATE
        "CRG_EXPORT=__attribute__((visibility(\"default\")))"
        "CRG_SHARED_LIB_PREFIX=\"lib\""
        "CRG_SHARED_LIB_EXTENSION=\".so\"")
    set_target_properties(crg_greeter_plugin PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(crg_greeter_plugin PRIVATE dl)
endif()

# ── Host test executable ──────────────────────────────────────────────────────
# main_runner.cpp is the Catch2 entry point shared by all public-tier test exes.
set(_MAIN_RUNNER "${CMAKE_CURRENT_SOURCE_DIR}/../core_monolith/main_runner.cpp")

add_executable(Run_PluginLoadLibrary_Tests
    stage_plugin_loadlibrary.test.cpp
    ${_MAIN_RUNNER}
)

target_link_libraries(Run_PluginLoadLibrary_Tests PRIVATE crg_core)

target_include_directories(Run_PluginLoadLibrary_Tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../external)

target_compile_definitions(Run_PluginLoadLibrary_Tests PRIVATE
    CRG_PLUGINS_ENABLED=1)

# Bake the plugin's output path at compile time so the test skips filesystem search.
target_compile_definitions(Run_PluginLoadLibrary_Tests PRIVATE
    "CRG_GREETER_PLUGIN_PATH=\"$<TARGET_FILE:crg_greeter_plugin>\"")

if(WIN32)
    target_compile_definitions(Run_PluginLoadLibrary_Tests PRIVATE
        "CRG_EXPORT=__declspec(dllimport)"
        "CRG_SHARED_LIB_PREFIX=\"\""
        "CRG_SHARED_LIB_EXTENSION=\".dll\"")
elseif(APPLE)
    target_compile_definitions(Run_PluginLoadLibrary_Tests PRIVATE
        "CRG_EXPORT=__attribute__((visibility(\"default\")))"
        "CRG_SHARED_LIB_PREFIX=\"lib\""
        "CRG_SHARED_LIB_EXTENSION=\".dylib\"")
else()
    target_compile_definitions(Run_PluginLoadLibrary_Tests PRIVATE
        "CRG_EXPORT=__attribute__((visibility(\"default\")))"
        "CRG_SHARED_LIB_PREFIX=\"lib\""
        "CRG_SHARED_LIB_EXTENSION=\".so\"")
    target_link_libraries(Run_PluginLoadLibrary_Tests PRIVATE dl)
endif()

# Ensure the plugin DLL is built before the test exe — the baked path must exist.
add_dependencies(Run_PluginLoadLibrary_Tests crg_greeter_plugin)
