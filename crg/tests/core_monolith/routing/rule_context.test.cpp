// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// ═════════════════════════════════════════════════════════════════════════════
// RULE CONTEXT — Find() never copies the RuleContext argument
// ─────────────────────────────────────────────────────────────────────────────
// RuleContext is passed through Find() by const reference all the way down to
// each cell's Condition() check. This test proves it with a copy-counting
// RuleContext: after Find() resolves across several candidate cells, the copy
// count must still be zero.
// ═════════════════════════════════════════════════════════════════════════════

#include "catch.hpp"
#include "crg/crg.hpp"

using namespace crg;
using namespace crg::models;
using namespace crg::routing;
using namespace crg::capabilities;

namespace crg::test::rule_context {

    struct AlertDomain {};
    struct AlertUnit { u8 m_Data[8]; };

    struct IAlert {
        struct Params { bool m_Raised{ false }; };
        struct RuleContext {
            int m_Level{ 0 };
            static inline int s_CopyCount = 0;

            explicit RuleContext(int level) : m_Level(level) {}
            RuleContext(const RuleContext& other) : m_Level(other.m_Level) {
                ++s_CopyCount;
            }
        };
    };

} // namespace crg::test::rule_context

CRG_DECLARE_DOMAIN(crg::test::rule_context::AlertDomain)
CRG_DEFINE_DOMAIN(crg::test::rule_context::AlertDomain)

CRG_DECLARE_DOMAIN_MODELS(crg::test::rule_context::AlertDomain,
    crg::test::rule_context::AlertUnit)

namespace crg::test::rule_context {

    struct AlertConfig {
        int m_MinLevel{ 0 };

        bool Condition(const IAlert::RuleContext& ctx) const {
            return ctx.m_Level >= m_MinLevel;
        }
    };

    template<typename TModel>
    struct RaiseAlert : public Capability<IAlert, AlertConfig> {
        RaiseAlert() {
            this->m_Config.m_MinLevel = 5;
        }
        static void Execute(IAlert::Params& p) { p.m_Raised = true; }
    };

    template<typename TModel>
    struct ClearAlert : public Capability<IAlert> {
        static void Execute(IAlert::Params& p) { p.m_Raised = false; }
    };

} // namespace crg::test::rule_context

namespace {
    using namespace crg::test::rule_context;
    static const CapabilityBinding<AlertDomain, AlertUnit, RaiseAlert, ClearAlert> s_alert;
}

TEST_CASE("Rule context — Find() does not copy RuleContext across candidate cells", "[rule_context][nocopy]") {
    using namespace crg::test::rule_context;
    IAlert::RuleContext::s_CopyCount = 0;

    auto token = ModelToken<AlertDomain>::FromType<AlertUnit>();
    auto gate  = CapabilityRouter<AlertDomain>::Find<IAlert>(token, IAlert::RuleContext{7});

    REQUIRE(gate);
    IAlert::Params p;
    gate(p);
    REQUIRE(p.m_Raised == true);

    REQUIRE(IAlert::RuleContext::s_CopyCount == 0);
}
