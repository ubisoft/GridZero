cmake_minimum_required(VERSION 3.15)

# Pure header library — no compilation unit. INTERFACE keeps consumers correct.
# VS generator never emits a .vcxproj for INTERFACE targets. The custom_target
# below is a header-only "project" node that VS will show in the solution explorer.
add_library(crg_core INTERFACE)

file(GLOB_RECURSE CRG_CORE_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.inl"
)

target_include_directories(crg_core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# Release = ship build — assertions disabled. Debug inherits the header default (1).
# Debug also turns on name retention (off by default, see config.hpp) so the
# FNV-1a / model-identity collision asserts in dense_index.hpp have a name to
# compare instead of silently comparing two default-constructed empty views.
target_compile_definitions(crg_core INTERFACE
    $<$<CONFIG:Release>:CRG_ASSERT_ENABLED=0>
    $<$<CONFIG:Debug>:CRG_HASH_NAME_ENABLED=1>
)

# VS-only visibility node: a no-op custom target whose SOURCES list makes all
# headers browsable in Solution Explorer. Has no build steps and no dependencies.
add_custom_target(crg_core_headers SOURCES ${CRG_CORE_HEADERS})

# DO NOT define CRG_PLUGINS_ENABLED here — core stays OS/plugin agnostic