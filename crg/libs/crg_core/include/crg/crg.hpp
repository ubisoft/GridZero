// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// crg.hpp — master include aggregating all CRG public headers

#pragma once

#include "crg/crg_core.hpp"
#include "crg/crg_discovery.hpp"
#include "crg/crg_models.hpp"
#include "crg/crg_type_erasure.hpp"
#include "crg/crg_routing.hpp"
#include "crg/crg_capabilities.hpp"

namespace crg {
    inline constexpr int VersionMajor = 1;
    inline constexpr int VersionMinor = 0;
    inline constexpr int VersionPatch = 0;
} // namespace crg
