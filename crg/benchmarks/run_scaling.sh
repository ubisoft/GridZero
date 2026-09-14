#!/bin/bash

# Copyright (c) Ubisoft. All Rights Reserved.
# Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

#chmod +x run_scaling.sh
set -e

GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}=======================================================${NC}"
echo -e "${BLUE}       CRG: COMPILER SCALING PROBE MATRIX              ${NC}"
echo -e "${BLUE}=======================================================${NC}\n"

# Make the python script executable if needed
chmod +x bench_compile_scaling.py

# Run the audit
python3 bench_compile_scaling.py

echo -e "\n${GREEN}=======================================================${NC}"
echo -e "${GREEN}   [OK] Compilation matrix complete.                     ${NC}"
echo -e "${GREEN}   Check 'scaling_report.md' for graph data.           ${NC}"
echo -e "${GREEN}=======================================================${NC}"
