// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// bench_hardware.hpp — shared hardware topology helpers for CRG benchmarks
//
// DenseThreadRange's upper bound must track the actual machine, not a
// hardcoded guess — an 8-thread cap on a 24-thread box silently stops the
// sweep long before the real memory-controller ceiling. It must also be
// PHYSICAL cores, not logical/SMT ones: past the physical core count, extra
// SMT siblings contend for the same execution units and caches instead of
// adding independent memory-bus access, which showed up as a noisy
// peak-then-decline in aggregate bandwidth around 8-12 threads on a
// 24-thread (SMT) machine, not a clean plateau.
//
// On Apple Silicon, hardware_concurrency() reports P+E cores combined, not
// physical performance cores — sysctl hw.perflevel0.physicalcpu (P-cores
// only) is queried first there. See BENCH_HARDWARE_NOTES.md.

#pragma once

#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#elif defined(__APPLE__)
#  include <sys/sysctl.h>
#endif

namespace crg::bench
{
    inline int HardwareThreadCeiling()
    {
#if defined(_WIN32)
        DWORD bufferSize = 0;
        GetLogicalProcessorInformation(nullptr, &bufferSize);
        if (bufferSize > 0)
        {
            std::vector<std::uint8_t> buffer(bufferSize);
            auto* info = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION*>(buffer.data());
            if (GetLogicalProcessorInformation(info, &bufferSize))
            {
                const std::size_t count = bufferSize / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
                int physicalCores = 0;
                for (std::size_t i = 0; i < count; ++i)
                {
                    if (info[i].Relationship == RelationProcessorCore)
                    {
                        ++physicalCores;
                    }
                }
                if (physicalCores > 0)
                {
                    return physicalCores;
                }
            }
        }
#elif defined(__APPLE__)
        int pCores = 0;
        std::size_t size = sizeof(pCores);
        if (sysctlbyname("hw.perflevel0.physicalcpu", &pCores, &size, nullptr, 0) == 0 && pCores > 0)
        {
            return pCores;
        }
#endif
        const unsigned int hc = std::thread::hardware_concurrency();
        return static_cast<int>(hc > 0 ? hc : 1);
    }
} // namespace crg::bench
