// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// bench_hardware.hpp — shared hardware topology helpers for CRG benchmarks
//
// DenseThreadRange's upper bound must track the actual machine, not a
// hardcoded guess — an 8-thread cap on a 24-thread box silently stops the
// sweep long before the real memory-controller ceiling.
//
// The sweep covers every LOGICAL thread the OS exposes: the goal is the
// maximum achievable throughput, and the point where SMT siblings or E-cores
// stop adding bandwidth (a visible knee) is itself a result worth plotting,
// not something to hide by capping the sweep. See BENCH_HARDWARE_NOTES.md for
// how to read heterogeneous (P/E-core) sweeps.

#pragma once

#include <thread>

namespace crg::bench
{
    inline int HardwareThreadCeiling()
    {
        const unsigned int hc = std::thread::hardware_concurrency();
        return static_cast<int>(hc > 0 ? hc : 1);
    }
} // namespace crg::bench
