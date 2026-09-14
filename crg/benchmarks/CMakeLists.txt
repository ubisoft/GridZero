# =============================================================================
# CRG MICRO-BENCHMARKS — Google Benchmark harness
# =============================================================================
#
# This subdirectory is OPTIONAL and OFF by default. Enable with:
#   cmake -B build -DCRG_BUILD_BENCHMARKS=ON
#
# Rationale: micro-benchmarks pull google/benchmark (~3 MB git) and require
# Release-mode CXX flags to be meaningful. Keeping the dependency opt-in
# preserves a fast default `cmake -B build` path for the unit-test pipeline.
# =============================================================================

cmake_minimum_required(VERSION 3.15)

include(FetchContent)

# ---- google/benchmark dependency --------------------------------------------
# Pinned to a recent stable tag to keep CI reproducible.
set(BENCHMARK_ENABLE_TESTING       OFF CACHE INTERNAL "")
set(BENCHMARK_ENABLE_GTEST_TESTS   OFF CACHE INTERNAL "")
set(BENCHMARK_ENABLE_INSTALL       OFF CACHE INTERNAL "")
set(BENCHMARK_ENABLE_WERROR        OFF CACHE INTERNAL "")
set(BENCHMARK_FORCE_WERROR         OFF CACHE INTERNAL "")
set(BENCHMARK_DOWNLOAD_DEPENDENCIES ON CACHE INTERNAL "")

FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.8.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(googlebenchmark)

# ---- Bench executable -------------------------------------------------------
file(GLOB_RECURSE CRG_BENCH_SOURCES
    "*.bench.cpp"
)

add_executable(Run_CRG_Benchmarks
    main_runner.cpp
    ${CRG_BENCH_SOURCES}
)

target_link_libraries(Run_CRG_Benchmarks PRIVATE
    crg_core
    benchmark::benchmark
)

# Enforce monolithic mode (parity with core_monolith tests).
target_compile_definitions(Run_CRG_Benchmarks PRIVATE
    CRG_PLUGINS_ENABLED=0
    CRG_SHARED_LIB_PREFIX="${CMAKE_SHARED_LIBRARY_PREFIX}"
    CRG_SHARED_LIB_EXTENSION="${CMAKE_SHARED_LIBRARY_SUFFIX}"
)

# ---- Release-grade flags ----------------------------------------------------
# Micro-benchmarks are meaningless without optimizations. We do NOT override
# CMAKE_BUILD_TYPE globally; we just enforce the minimum necessary on this
# target so a Debug top-level build still yields valid bench numbers.
if (MSVC)
    target_compile_options(Run_CRG_Benchmarks PRIVATE /O2 /Oi /GL)
    target_link_options   (Run_CRG_Benchmarks PRIVATE /LTCG)
else()
    target_compile_options(Run_CRG_Benchmarks PRIVATE
        -O3 -fno-omit-frame-pointer -DNDEBUG
    )
endif()

# Surface the binary path so run_benchmarks.sh stays aligned with build/.
set_target_properties(Run_CRG_Benchmarks PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/crg/benchmarks
)
