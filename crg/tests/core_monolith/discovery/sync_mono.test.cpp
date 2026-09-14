// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// =============================================================================
// UNIT TEST: MONOLITHIC PARTITION DISCOVERY & POPULATORS
// =============================================================================

#include "catch.hpp"
#include "crg/crg_discovery.hpp"
#include "crg/routing/capability_router.hpp"
#include "crg/core/domain_macros.hpp"
#include "crg/capabilities/capability_binding.hpp"
#include "crg/models/model_token.hpp"

namespace crg::test::mono {
    using namespace crg::discovery;
    using namespace crg::routing;

    struct GraphicsPartition {};

    // Concrete Arena Populator for this domain
    struct MockGraphicsPopulator : public DomainArenaPopulator<GraphicsPartition> {
        void Populate() override { /* Routing rule injection logic */ }
    };
}

// =============================================================================
// STRICT ENFORCEMENT REGISTRATION
// =============================================================================

CRG_DECLARE_DOMAIN(::crg::test::mono::GraphicsPartition)
CRG_DEFINE_DOMAIN(::crg::test::mono::GraphicsPartition)

namespace {
    static const crg::test::mono::MockGraphicsPopulator s_GraphicsPopulator;
}

// =============================================================================
// TEST SUITES
// =============================================================================

TEST_CASE("Discovery: Monolithic local domain populator", "[crg][mono]") {
    using namespace crg::test::mono;

    // 1. Setup: Ensure router handles lazy bake configuration
    CapabilityRouter<GraphicsPartition>::RefreshCache();

    // 2. Verification: In monolithic mode, the populator is auto-registered to the cache
    int nodeCount = 0;
    DomainArenaPopulator<GraphicsPartition>::Visit([&nodeCount](const DomainArenaPopulator<GraphicsPartition>&) {
        nodeCount++;
    });

    REQUIRE(nodeCount == 1);
}

// =============================================================================
// RefreshCache idempotence — Clear-then-Populate must not accumulate dynamic
// rules across repeated refreshes (regression guard for the Volet 2 fix).
// =============================================================================

namespace crg::test::mono {
    struct RefreshDomain {};
    struct RefreshUnit { u8 m_Data[8]; };
}

CRG_DECLARE_DOMAIN(::crg::test::mono::RefreshDomain)
CRG_DEFINE_DOMAIN(::crg::test::mono::RefreshDomain)
CRG_DECLARE_DOMAIN_MODELS(::crg::test::mono::RefreshDomain,
    ::crg::test::mono::RefreshUnit)

namespace crg::test::mono {

    struct IRefreshRule {
        struct Params { bool m_Fired{ false }; };
        struct RuleContext {
            int m_Level{ 0 };
            explicit RuleContext(int l) : m_Level(l) {}
        };
    };

    struct RefreshRuleConfig {
        bool Condition(const IRefreshRule::RuleContext& ctx) const {
            return ctx.m_Level >= 0;
        }
    };

    template<typename TModel>
    struct DynamicRefreshRule : public ::crg::capabilities::Capability<IRefreshRule, RefreshRuleConfig> {
        static void Execute(IRefreshRule::Params& p) { p.m_Fired = true; }
    };

    static const ::crg::capabilities::CapabilityBinding<RefreshDomain, RefreshUnit, DynamicRefreshRule> s_RefreshBinding;
}

TEST_CASE("Discovery: RefreshCache is idempotent across repeated Clear+Populate", "[crg][mono][refresh]") {
    using namespace crg::test::mono;

    auto token = ::crg::models::ModelToken<RefreshDomain>::FromType<RefreshUnit>();
    auto gate  = CapabilityRouter<RefreshDomain>::Find<IRefreshRule>(token, IRefreshRule::RuleContext{ 1 });
    REQUIRE(gate);

    const auto* cellsBefore = ::crg::routing::TensorArena<RefreshDomain, IRefreshRule>::GetData();
    const std::size_t rulesBefore = cellsBefore[0].m_DynamicRules.size();
    REQUIRE(rulesBefore == 1);

    CapabilityRouter<RefreshDomain>::RefreshCache();
    CapabilityRouter<RefreshDomain>::RefreshCache();

    const auto* cellsAfter = ::crg::routing::TensorArena<RefreshDomain, IRefreshRule>::GetData();
    const std::size_t rulesAfter = cellsAfter[0].m_DynamicRules.size();
    REQUIRE(rulesAfter == 1);
}
