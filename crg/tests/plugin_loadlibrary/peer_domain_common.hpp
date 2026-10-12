// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

#pragma once
#include "crg/crg.hpp"
#include "crg/capabilities/capability.hpp"
#include "crg/routing/capability_routing_traits.hpp"
#include "crg/routing/capability_space.hpp"
#include "crg/core/at.hpp"

struct PeerDomain {};
CRG_DECLARE_DOMAIN(PeerDomain)

struct PeerContract {
    struct Params { int& m_Value; };
};
CRG_DECLARE_CONTRACT(PeerContract)

namespace crg::routing {
    template<> struct CapabilityRoutingTraits<PeerDomain, PeerContract> {
        using SpaceType = CapabilitySpace<>;
    };
}

struct PeerModel {};
CRG_DECLARE_DOMAIN_MODELS(PeerDomain, PeerModel)
