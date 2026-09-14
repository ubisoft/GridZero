// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// domain_key.hpp — semantic tag identifying a Domain type

#pragma once
#include "crg/core/hash.hpp"

namespace crg::models {

    // Intentionally incomplete; specialize only via CRG_DECLARE_DOMAIN.
    template<class TDomain>
    struct DomainKey;

}
