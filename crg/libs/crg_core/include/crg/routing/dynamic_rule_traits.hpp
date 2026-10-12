// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// dynamic_rules_traits.hpp — compile-time identity and model-key contract for domains

#pragma once

#include <vector>

namespace crg::routing {

    // Forward declaration to allow traits to take it as parameter
    template<typename TDomain, typename TContract>
    class DynamicRule;

    template<class TDomain, class TContract>
    struct DynamicRuleTraits {
        using ConfigBaseType = void; 
        
        static void SortRules(std::vector<DynamicRule<TDomain, TContract>>&) {}
    };
}