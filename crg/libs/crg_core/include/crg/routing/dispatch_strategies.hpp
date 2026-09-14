// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// dispatch_strategies.hpp — Alternative dispatch cells for capability routing

#pragma once
#include "crg/capabilities/binding_target.hpp"
#include "crg/capabilities/capability.hpp"
#include "crg/capabilities/capability_handle.hpp"
#include "crg/routing/capability_routing_traits.hpp"
#include <vector>
#include <limits>

namespace crg::routing {

    template<typename TDomain, typename TContract>
    struct UtilityScoringCell {
        using ContextType = ContextTypeOf<TDomain, TContract>;
        using CapHandle   = ::crg::capabilities::CapabilityHandle<TContract>;
        using Target      = ::crg::capabilities::BindingTarget<TContract>;
        using EvaluatorFn = float (*)(const void*, const ContextType&);

        struct Scorer {
            Target      m_Target{};
            const void* m_ConfigData{ nullptr };
            EvaluatorFn m_Evaluator{ nullptr };
        };

        std::vector<Scorer> m_Options{};
        Target m_Fallback{};

        template<class Impl>
        void Bind(const Target& target, const Impl* instance) {
            if constexpr (::crg::capabilities::HasConfigType<Impl>::Value) {
                auto tramp = [](const void* obj, const ContextType& ctx) -> float {
                    return static_cast<const Impl*>(obj)->m_Config.Evaluate(ctx);
                };
                m_Options.push_back({ target, static_cast<const void*>(&instance->m_Config), tramp });
            } else {
                if constexpr (std::is_base_of_v<TContract, Impl> ||
                              std::is_base_of_v<::crg::capabilities::Capability<TContract>, Impl>) {
                    m_Fallback = target;
                }
            }
        }

        CapHandle Resolve(const ContextType& ctx) const {
            // &m_Fallback is never null; its own Target pointer defaults to
            // nullptr, so an unbound cell still resolves to a correctly-null
            // CapHandle below without a separate "has fallback" bool.
            const Target* best = &m_Fallback;
            float maxScore = -std::numeric_limits<float>::infinity();

            for (const auto& scorer : m_Options) {
                float score = scorer.m_Evaluator(scorer.m_ConfigData, ctx);
                if (score > maxScore) {
                    maxScore = score;
                    best = &scorer.m_Target;
                }
            }

            CapHandle r; r = best; return r;
        }
    };

}
