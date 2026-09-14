// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// branchless_dispatch_cell.hpp — SoA bitmask dispatch cell for capability routing

#pragma once
#include <cstdint>
#include "crg/capabilities/binding_target.hpp"
#include "crg/capabilities/capability.hpp"
#include "crg/capabilities/capability_handle.hpp"
#include "crg/routing/capability_routing_traits.hpp"

namespace crg::routing {

// Precondition: mask != 0 (callers must guard with `if (validMask != 0)`).
[[nodiscard]] inline uint32_t CountTrailingZeros64(uint64_t mask) noexcept {
    uint32_t n = 0;
    if ((mask & 0xFFFFFFFFULL) == 0) { n += 32; mask >>= 32; }
    if ((mask & 0xFFFFULL)     == 0) { n += 16; mask >>= 16; }
    if ((mask & 0xFFULL)       == 0) { n +=  8; mask >>=  8; }
    if ((mask & 0xFULL)        == 0) { n +=  4; mask >>=  4; }
    if ((mask & 0x3ULL)        == 0) { n +=  2; mask >>=  2; }
    if ((mask & 0x1ULL)        == 0) { n +=  1;              }
    return n;
}

template<typename TDomain, typename TContract>
struct BranchlessDispatchCell {
    using Target      = ::crg::capabilities::BindingTarget<TContract>;
    using CapHandle   = ::crg::capabilities::CapabilityHandle<TContract>;
    using Params      = typename TContract::Params;
    using ContextType = ContextTypeOf<TDomain, TContract>;
    using ExecFn      = void (*)(Params&);
    using CondFn      = bool (*)(const ContextType&);

    static constexpr uint32_t MaxRules = 64;

    uint32_t m_RuleCount{0};

    CondFn   m_Conditions[MaxRules]{};
    ExecFn   m_Executes[MaxRules]{};
    ExecFn   m_FallbackExecute{nullptr};

    template<class Impl, class TTarget>
    void Bind(const TTarget& target, const Impl* /*implPtr*/) {
        if constexpr (!::crg::capabilities::HasConfigType<Impl>::Value) {
            m_FallbackExecute = target.m_Target;
        } else {
            if (m_RuleCount < MaxRules) {
                m_Executes[m_RuleCount] = target.m_Target;
                // TConfig must be default-constructible and stateless; all constraint
                // values must be static constexpr fields — lambda-to-fptr requires it.
                m_Conditions[m_RuleCount] = [](const ContextType& ctx) -> bool {
                    static const typename Impl::ConfigType s_cfg{};
                    return s_cfg.Condition(ctx);
                };
                ++m_RuleCount;
            }
        }
    }

    void EvaluateAndExecute(const ContextType& ctx, Params& p) const noexcept {
        uint64_t validMask = 0;
        for (uint32_t i = 0; i < m_RuleCount; ++i)
            validMask |= (static_cast<uint64_t>(m_Conditions[i](ctx)) << i);

        if (validMask != 0) {
            m_Executes[CountTrailingZeros64(validMask)](p);
        } else if (m_FallbackExecute) {
            m_FallbackExecute(p);
        }
    }

    [[nodiscard]] CapHandle Resolve(const ContextType& ctx) const noexcept {
        uint64_t validMask = 0;
        for (uint32_t i = 0; i < m_RuleCount; ++i)
            validMask |= (static_cast<uint64_t>(m_Conditions[i](ctx)) << i);

        Target t{};
        if (validMask != 0) {
            t.m_Target = m_Executes[CountTrailingZeros64(validMask)];
        } else if (m_FallbackExecute) {
            t.m_Target = m_FallbackExecute;
        } else {
            return CapHandle{};
        }
        CapHandle h; h = &t; return h;
    }
};

} // namespace crg::routing
