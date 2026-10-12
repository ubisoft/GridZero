// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

#pragma once
#include "crg/crg.hpp"
#include "crg/discovery/node_link.hpp"
#include "crg/capabilities/capability_binding.hpp"

struct GreeterDomain {};
CRG_DECLARE_DOMAIN(GreeterDomain)

struct IGreet {
    struct Params { const char* m_Out{ nullptr }; };
};
CRG_DECLARE_CONTRACT(IGreet)

struct IGreeterRule {
    struct RuleContext { int m_Urgency{ 0 }; };
    struct Params      { const char* m_Message{ nullptr }; int m_Priority{ -1 }; };
};
CRG_DECLARE_CONTRACT(IGreeterRule)

struct IGreeterObserver {
    virtual const char* GetObserverName() const = 0;
    virtual ~IGreeterObserver() = default;
};

struct GreeterObserverNode : public crg::discovery::NodeLink<GreeterObserverNode, IGreeterObserver> {};
CRG_DECLARE_NODE_LINK(GreeterObserverNode)

struct English {};
struct French  {};

CRG_DECLARE_DOMAIN_MODELS(GreeterDomain, English, French)

struct GreeterRuleConfig {
    int m_MinUrgency{ 0 };
    int m_Priority{ 0 };

    bool Condition(const IGreeterRule::RuleContext& ctx) const {
        return ctx.m_Urgency >= m_MinUrgency;
    }
};

namespace crg::routing {
    template<>
    struct DynamicRuleTraits<GreeterDomain, IGreeterRule> {
        using ConfigBaseType = GreeterRuleConfig;

        static void SortRules(std::vector<DynamicRule<GreeterDomain, IGreeterRule>>& rules) {
            std::stable_sort(rules.begin(), rules.end(), [](const auto& a, const auto& b) {
                const auto* cfgA = a.GetConfig();
                const auto* cfgB = b.GetConfig();

                int prioA = cfgA ? cfgA->m_Priority : -1;
                int prioB = cfgB ? cfgB->m_Priority : -1;

                // Descending sort (highest priority first)
                return prioA > prioB; 
            });
        }
    };
}
