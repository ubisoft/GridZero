#!/bin/bash

# Copyright (c) Ubisoft. All Rights Reserved.
# Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

# =============================================================================
# CRG BENCHMARK AGGREGATOR LAUNCHER (macOS / Linux)
# =============================================================================
# Usage:
#   ./run_aggregate.sh --dir results/
#   ./run_aggregate.sh file1.json file2.json --out report.md
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
python3 "${SCRIPT_DIR}/bench_aggregate.py" "$@"
