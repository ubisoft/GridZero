#!/bin/bash

# Copyright (c) Ubisoft. All Rights Reserved.
# Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

# Mac / Linux launcher for the Python orchestrator
# ./run_benchmarks.sh --out-dir results/
# ./run_benchmarks.sh --filter "BM_CapabilityRouter" --out-dir results/

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
python3 "${SCRIPT_DIR}/bench_orchestrator.py" "$@"
