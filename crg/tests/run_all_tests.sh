#!/bin/bash

# Copyright (c) Ubisoft. All Rights Reserved.
# Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.


# --- Terminal Colors ---
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

# Get the absolute path of the 'tests' directory
TESTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${CRG_BIN_DIR:-$TESTS_DIR/../../build}"

echo -e "${BLUE}=======================================================${NC}"
echo -e "${BLUE}       CRG: EXECUTING ALL PUBLIC-TIER TEST SUITES       ${NC}"
echo -e "${BLUE}=======================================================${NC}\n"

run() {
    local n="$1" total="$2" label="$3" cmd="$4"
    echo -e "${BLUE}[$n/$total] Starting $label...${NC}"
    eval "$cmd"
    if [ $? -ne 0 ]; then
        echo -e "\n${RED}[FAIL] $label failed. Aborting.${NC}"
        exit 1
    fi
    echo -e "\n"
}

# Public tier ships two test targets: Core Monolith and the manual-load plugin
# stage. Everything else lives only in the protected tier - see README.md
# "What is in this repository".
run 1 2 "Core Monolith Tests"     '"$BUILD_DIR/crg/tests/core_monolith/Run_CoreMonolith_Tests"'
run 2 2 "Plugin LoadLibrary Tests" '"$BUILD_DIR/crg/tests/plugin_loadlibrary/Run_PluginLoadLibrary_Tests"'

echo -e "${GREEN}=======================================================${NC}"
echo -e "${GREEN}       [OK] ALL CRG PUBLIC TEST SUITES PASSED!            ${NC}"
echo -e "${GREEN}=======================================================${NC}"
