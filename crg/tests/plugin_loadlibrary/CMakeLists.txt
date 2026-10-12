# stage_plugin_loadlibrary — Manual-load plugin stage (CppCon Acte 1 demo)
#
# Two targets:
#   crg_greeter_plugin             — shared library (.dll/.so) loaded at runtime
#   Run_PluginLoadLibrary_Tests    — host executable; uses its own LoadLibrary call
#
# Neither target links any GridZero .cpp — crg_core is header-only.

cmake_minimum_required(VERSION 3.15)

# ── Plugin DLL ────────────────────────────────────────────────────────────────
crg_add_plugin(crg_greeter_plugin plugin_greeter.cpp)
crg_add_plugin(crg_shared_state_a plugin_shared_state.cpp)
crg_add_plugin(crg_shared_state_b plugin_shared_state.cpp)
crg_add_plugin(crg_shared_state_c plugin_shared_state.cpp)
crg_add_plugin(crg_shared_state_d plugin_shared_state.cpp)
crg_add_plugin(crg_order_a plugin_order.cpp)
crg_add_plugin(crg_order_b plugin_order.cpp)
foreach(_role 0 1)
    crg_add_plugin(crg_join_r${_role} plugin_join.cpp)
    target_compile_definitions(crg_join_r${_role} PRIVATE JOIN_ROLE=${_role})
endforeach()
foreach(_peer a b c)
    crg_add_plugin(crg_peer_${_peer} plugin_peer_chain.cpp)
endforeach()
foreach(_dom a b c)
    crg_add_plugin(crg_domain_${_dom} plugin_peer_domain.cpp)
endforeach()
target_compile_definitions(crg_domain_a PRIVATE PEER_ROLE=0)
target_compile_definitions(crg_domain_b PRIVATE PEER_ROLE=1)
target_compile_definitions(crg_domain_c PRIVATE PEER_ROLE=2)
target_compile_definitions(crg_peer_a PRIVATE PEER_ID=1)
target_compile_definitions(crg_peer_b PRIVATE PEER_ID=2)
target_compile_definitions(crg_peer_c PRIVATE PEER_ID=4)
target_compile_definitions(crg_order_a PRIVATE ORDER_VARIANT=0)
target_compile_definitions(crg_order_b PRIVATE ORDER_VARIANT=1)

# ── Host test executable ──────────────────────────────────────────────────────
set(_MAIN_RUNNER "${CMAKE_CURRENT_SOURCE_DIR}/../core_monolith/main_runner.cpp")

crg_add_host(Run_PluginLoadLibrary_Tests 
    stage_plugin_loadlibrary.test.cpp 
    ${_MAIN_RUNNER}
)

target_include_directories(Run_PluginLoadLibrary_Tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../external)

# Bake the plugin's output path at compile time so the test skips filesystem search.
target_compile_definitions(Run_PluginLoadLibrary_Tests PRIVATE
    "CRG_GREETER_PLUGIN_PATH=\"$<TARGET_FILE:crg_greeter_plugin>\""
    "CRG_SHARED_STATE_A_PATH=\"$<TARGET_FILE:crg_shared_state_a>\""
    "CRG_SHARED_STATE_B_PATH=\"$<TARGET_FILE:crg_shared_state_b>\""
    "CRG_SHARED_STATE_C_PATH=\"$<TARGET_FILE:crg_shared_state_c>\""
    "CRG_SHARED_STATE_D_PATH=\"$<TARGET_FILE:crg_shared_state_d>\""
    "CRG_ORDER_A_PATH=\"$<TARGET_FILE:crg_order_a>\""
    "CRG_ORDER_B_PATH=\"$<TARGET_FILE:crg_order_b>\""
    "CRG_JOIN_R0_PATH=\"$<TARGET_FILE:crg_join_r0>\"" "CRG_JOIN_R1_PATH=\"$<TARGET_FILE:crg_join_r1>\""
    "CRG_PEER_A_PATH=\"$<TARGET_FILE:crg_peer_a>\"" "CRG_PEER_B_PATH=\"$<TARGET_FILE:crg_peer_b>\"" "CRG_PEER_C_PATH=\"$<TARGET_FILE:crg_peer_c>\""
    "CRG_DOMAIN_A_PATH=\"$<TARGET_FILE:crg_domain_a>\"" "CRG_DOMAIN_B_PATH=\"$<TARGET_FILE:crg_domain_b>\"" "CRG_DOMAIN_C_PATH=\"$<TARGET_FILE:crg_domain_c>\"")

# Ensure the plugin DLL is built before the test exe — the baked path must exist.
add_dependencies(Run_PluginLoadLibrary_Tests crg_greeter_plugin crg_shared_state_a crg_shared_state_b crg_shared_state_c crg_shared_state_d crg_order_a crg_order_b crg_join_r0 crg_join_r1 crg_peer_a crg_peer_b crg_peer_c crg_domain_a crg_domain_b crg_domain_c)

# Ensure this module is registered for debug
add_test(NAME Run_PluginLoadLibrary_Tests COMMAND Run_PluginLoadLibrary_Tests)

# Force working directory to the target binary folder so dlopen finds the library
set_tests_properties(Run_PluginLoadLibrary_Tests PROPERTIES WORKING_DIRECTORY $<TARGET_FILE_DIR:Run_PluginLoadLibrary_Tests>)