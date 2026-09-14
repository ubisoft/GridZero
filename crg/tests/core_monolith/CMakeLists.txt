# =============================================================================
# THE MONOLITH (Pure Core Algorithm Tests) - PUBLIC tier
# =============================================================================
# Only stages 00-07 + the core/discovery/models/routing/type_erasure suites
# were copied to public (stages 08-10 depend on the protected engine tier).

file(GLOB_RECURSE MONO_TEST_SOURCES
    "*.test.cpp"
    "test_*.cpp"
)

add_executable(Run_CoreMonolith_Tests
    main_runner.cpp
    ${MONO_TEST_SOURCES}
)

# PUBLIC: crg_core ONLY. DO NOT link the protected memory tier - verified: no
# kept stage 00-07 includes crg/memory (public leak check = 0).
target_link_libraries(Run_CoreMonolith_Tests PRIVATE crg_core)

target_include_directories(Run_CoreMonolith_Tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../external)

# Strict monolithic mode (parity with the old behavior).
target_compile_definitions(Run_CoreMonolith_Tests PRIVATE CRG_PLUGINS_ENABLED=0)
