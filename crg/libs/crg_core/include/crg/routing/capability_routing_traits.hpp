// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// capability_routing_traits.hpp — compile-time routing context traits for capabilities

#pragma once
#include "crg/routing/capability_space.hpp"
#include <type_traits>

namespace crg::routing {

    struct NullContext {
        NullContext() = default;
    };

    template<class TDomain, class TContract>
    struct CapabilityRoutingTraits {
        using SpaceType = CapabilitySpace<>;
    };

    namespace internal {

        // Safe lazy extraction: avoids std::conditional_t eager instantiation of both branches
        template<typename T, typename = void>
        struct ExtractRuleContext {
            using Type = NullContext;
        };

        template<typename T>
        struct ExtractRuleContext<T, std::void_t<typename T::RuleContext>> {
            using Type = typename T::RuleContext;
        };

        template<typename TDomain, typename TContract, typename = void>
        struct ContextSelector {
            using Type = typename ExtractRuleContext<TContract>::Type;
        };

        template<typename TDomain, typename TContract>
        struct ContextSelector<
            TDomain,
            TContract,
            std::void_t<typename CapabilityRoutingTraits<TDomain, TContract>::RuleContext>
        > {
            using Type = typename CapabilityRoutingTraits<TDomain, TContract>::RuleContext;

            static_assert(
                std::is_same_v<typename ExtractRuleContext<TContract>::Type, NullContext> ||
                std::is_same_v<typename ExtractRuleContext<TContract>::Type, Type>,
                "CRG Error: CapabilityRoutingTraits<TDomain, TContract>::RuleContext overrides a "
                "RuleContext type different from the one TContract declares itself. "
                "Keep a single RuleContext definition per contract.");
        };

    } // namespace internal

    template<typename TDomain, typename TContract>
    using ContextTypeOf = typename internal::ContextSelector<TDomain, TContract>::Type;

    template<typename TDomain, typename TContract>
    inline constexpr bool HasDynamicRules = !std::is_same_v<ContextTypeOf<TDomain, TContract>, NullContext>;

    template<typename TDomain, typename TContract, bool THasRule = HasDynamicRules<TDomain, TContract>>
    struct FullContext;

    template<typename TDomain, typename TContract>
    struct FullContext<TDomain, TContract, true> {
        using Base = ContextTypeOf<TDomain, TContract>;
        static constexpr bool RequiresContext = true;
    };

    template<typename TDomain, typename TContract>
    struct FullContext<TDomain, TContract, false> {
        using Base = NullContext;
        static constexpr bool RequiresContext = false;
    };

} // namespace crg::routing
