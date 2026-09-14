// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// =============================================================================
// CRG TEST RUNNER - AGGREGATED UNIT TESTS
// =============================================================================

#if defined(_WIN32)
#include <cstdlib>

namespace {
    // A firing CRG_ASSERT (assert() -> abort()) would otherwise pop the CRT's
    // blocking "Debug Error" dialog, hanging the whole run with no one there
    // to dismiss it. Force a silent abort (message on stderr, non-zero exit)
    // instead. Runs at static init, before main() — guaranteed ahead of any
    // test — because it lives in this TU, ahead of the CATCH_CONFIG_MAIN
    // expansion below.
    struct SilentAbortOnCrash {
        SilentAbortOnCrash() {
            _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        }
    };
    const SilentAbortOnCrash s_SilentAbortOnCrash{};
}
#endif

#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "crg/core/scoped_no_alloc.hpp"
CRG_INSTALL_NO_ALLOC_GUARD()
