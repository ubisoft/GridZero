cmake_minimum_required(VERSION 3.15)
project(GridZero)

# =============================================================================
# GridZero - public tier (Apache 2.0). Header-only crg_core.
# The WASM backend was moved into the protected tier (non-public).
# =============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/EHsc)
endif()

# OS-agnostic build-system injection - replaces every #ifdef OS in the headers.
if(WIN32)
    add_compile_definitions(
        CRG_EXPORT=__declspec\(dllexport\)
        CRG_SHARED_LIB_PREFIX=""
        CRG_SHARED_LIB_EXTENSION=".dll"
    )
elseif(APPLE)
    add_compile_definitions(
        "CRG_EXPORT=__attribute__((visibility(\"default\")))"
        CRG_SHARED_LIB_PREFIX="lib"
        CRG_SHARED_LIB_EXTENSION=".dylib"
    )
else()  # Linux / POSIX
    add_compile_definitions(
        "CRG_EXPORT=__attribute__((visibility(\"default\")))"
        CRG_SHARED_LIB_PREFIX="lib"
        CRG_SHARED_LIB_EXTENSION=".so"
    )
endif()

# DO NOT define CRG_PLUGINS_ENABLED globally: each target sets it itself.
option(CRG_BUILD_BENCHMARKS "Build CRG micro-benchmarks (Google Benchmark)" OFF)

add_subdirectory(crg/libs/crg_core)
# The WASM backend lives in the protected tier - do not add_subdirectory it here.

# OFF when included as submodule by gridzero-protected (which owns the superset test target)
option(CRG_BUILD_PUBLIC_TESTS "Build public-tier test suite" ON)
if(CRG_BUILD_PUBLIC_TESTS)
    add_subdirectory(crg/tests)
endif()

if(CRG_BUILD_BENCHMARKS)
    add_subdirectory(crg/benchmarks)
endif()
