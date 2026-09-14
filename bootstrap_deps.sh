#!/bin/bash

# Copyright (c) Ubisoft. All Rights Reserved.
# Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

# =============================================================================
# GridZero - External dependency bootstrap for Sharpmake builds
# =============================================================================
# Downloads and vendors third-party header-only / pre-built libraries into
# external/ so Sharpmake projects can include them without FetchContent.
#
# Usage:
#   bash bootstrap_deps.sh          # download all deps
#   bash bootstrap_deps.sh --check  # verify deps exist, exit 1 if missing
#
# Deps managed:
#   external/nlohmann_json/include/nlohmann/json.hpp  (v3.11.3, header-only)
#   external/httplib/httplib.h                        (v0.14.3, header-only)
#   external/benchmark/include/                       (v1.8.3, built from source)
#   crg/tests/external/catch.hpp                      (v2.13.10, downloaded from Catch2 releases)
#
# wasmtime and WASM guest compilation are NOT needed here - the WASM backend
# and the tests that depend on it live only in the protected tier.
# =============================================================================

set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
EXTERNAL="$ROOT/external"
CHECK_ONLY=0
[[ "${1:-}" == "--check" ]] && CHECK_ONLY=1

NLOHMANN_VERSION="v3.11.3"
HTTPLIB_VERSION="v0.14.3"
BENCHMARK_VERSION="v1.8.3"

fail_check() { echo "MISSING: $1 - run bootstrap_deps.sh to download"; exit 1; }

# =============================================================================
# 1. nlohmann/json - single-header download
# =============================================================================
NLOHMANN_DIR="$EXTERNAL/nlohmann_json/include/nlohmann"
NLOHMANN_HEADER="$NLOHMANN_DIR/json.hpp"

if [[ $CHECK_ONLY -eq 1 ]]; then
    [[ -f "$NLOHMANN_HEADER" ]] || fail_check "$NLOHMANN_HEADER"
else
    if [[ ! -f "$NLOHMANN_HEADER" ]]; then
        echo ">>> Downloading nlohmann/json $NLOHMANN_VERSION..."
        mkdir -p "$NLOHMANN_DIR"
        curl --retry 3 -fsSL \
            "https://github.com/nlohmann/json/releases/download/${NLOHMANN_VERSION}/json.hpp" \
            -o "$NLOHMANN_HEADER"
        echo "    -> $NLOHMANN_HEADER"
    else
        echo "--- nlohmann/json already present, skipping"
    fi
fi

# =============================================================================
# 2. cpp-httplib - single-header download
# =============================================================================
HTTPLIB_HEADER="$EXTERNAL/httplib/httplib.h"

if [[ $CHECK_ONLY -eq 1 ]]; then
    [[ -f "$HTTPLIB_HEADER" ]] || fail_check "$HTTPLIB_HEADER"
else
    if [[ ! -f "$HTTPLIB_HEADER" ]]; then
        echo ">>> Downloading cpp-httplib $HTTPLIB_VERSION..."
        mkdir -p "$EXTERNAL/httplib"
        curl --retry 3 -fsSL \
            "https://raw.githubusercontent.com/yhirose/cpp-httplib/${HTTPLIB_VERSION}/httplib.h" \
            -o "$HTTPLIB_HEADER"
        echo "    -> $HTTPLIB_HEADER"
    else
        echo "--- cpp-httplib already present, skipping"
    fi
fi

# =============================================================================
# 3. Google Benchmark - clone + build (Release, no tests)
# =============================================================================
BENCHMARK_ROOT="$EXTERNAL/benchmark"
BENCHMARK_INCLUDE="$BENCHMARK_ROOT/include/benchmark/benchmark.h"
BENCHMARK_LIB="$BENCHMARK_ROOT/lib/libbenchmark.a"

if [[ $CHECK_ONLY -eq 1 ]]; then
    [[ -f "$BENCHMARK_INCLUDE" ]] || fail_check "$BENCHMARK_INCLUDE"
    [[ -f "$BENCHMARK_LIB" ]]    || fail_check "$BENCHMARK_LIB"
else
    if [[ ! -f "$BENCHMARK_LIB" ]]; then
        echo ">>> Building google/benchmark $BENCHMARK_VERSION..."
        TMP="$(mktemp -d)"
        git clone --depth=1 --branch "$BENCHMARK_VERSION" \
            https://github.com/google/benchmark.git "$TMP/benchmark"
        cmake -S "$TMP/benchmark" -B "$TMP/build" \
            -DBENCHMARK_ENABLE_TESTING=OFF \
            -DBENCHMARK_ENABLE_GTEST_TESTS=OFF \
            -DBENCHMARK_ENABLE_INSTALL=OFF \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_INSTALL_PREFIX="$BENCHMARK_ROOT"
        cmake --build "$TMP/build" --config Release --parallel
        cmake --install "$TMP/build"
        rm -rf "$TMP"
        echo "    -> $BENCHMARK_ROOT"
    else
        echo "--- google/benchmark already present, skipping"
    fi
fi

# =============================================================================
# 4. Catch2 single-header - crg/tests/external/catch.hpp
# =============================================================================
CATCH_HEADER="$ROOT/crg/tests/external/catch.hpp"

if [[ $CHECK_ONLY -eq 1 ]]; then
    [[ -f "$CATCH_HEADER" ]] || fail_check "$CATCH_HEADER"
else
    if [[ ! -f "$CATCH_HEADER" ]]; then
        echo ">>> Downloading Catch2 single-header..."
        mkdir -p "$(dirname "$CATCH_HEADER")"
        curl --retry 3 -fsSL \
            "https://github.com/catchorg/Catch2/releases/download/v2.13.10/catch.hpp" \
            -o "$CATCH_HEADER"
        echo "    -> $CATCH_HEADER"
    else
        echo "--- Catch2 already present, skipping"
    fi
fi

# =============================================================================
# Summary
# =============================================================================
echo ""
echo "=== external/ dependency status ==="
[[ -f "$NLOHMANN_HEADER"  ]] && echo "[OK] nlohmann/json"    || echo "[FAIL] nlohmann/json"
[[ -f "$HTTPLIB_HEADER"   ]] && echo "[OK] cpp-httplib"      || echo "[FAIL] cpp-httplib"
[[ -f "$BENCHMARK_LIB"   ]] && echo "[OK] google/benchmark" || echo "[FAIL] google/benchmark (optional)"
[[ -f "$CATCH_HEADER"     ]] && echo "[OK] catch2"           || echo "[FAIL] catch2"
echo ""
