// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// Plugin DLL compiled three times (crg_domain_a .. c). The host defines no domain. PEER_ROLE 0 only defines the domain,
// 1 also contributes a capability to it, 2 only consumes. CRG_DomainCall resolves the contract through the router of
// the module and returns the value it produces: -1 when the model is unknown, -2 when no capability is bound.

#include "crg/crg_plugin.hpp"
#include "crg/crg.hpp"
#include "crg/capabilities/capability_binding.hpp"
#include "peer_domain_common.hpp"

CRG_DEFINE_PLUGIN(PeerDomainPlugin)
CRG_DEFINE_DOMAIN(PeerDomain)

#if PEER_ROLE == 1
namespace {
    template<class TModel, class TAt = crg::At<>>
    struct PeerLogic : crg::capabilities::Capability<PeerContract> {
        static void Execute(PeerContract::Params& p) { p.m_Value += 10; }
    };

    const crg::capabilities::CapabilityBinding<PeerDomain, PeerModel, PeerLogic> s_Binding{};
}
#endif

extern "C" CRG_EXPORT int CRG_DomainCall() {
    const auto token = crg::models::ModelToken<PeerDomain>::FromType<PeerModel>();
    if (!token.IsValid()) return -1;
    auto handle = crg::routing::CapabilityRouter<PeerDomain>::Find<PeerContract>(token);
    if (!handle) return -2;
    int value = 0;
    PeerContract::Params params{value};
    handle(params);
    return value;
}
