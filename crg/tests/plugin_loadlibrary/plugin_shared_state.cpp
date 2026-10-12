// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// Plugin DLL compiled four times (crg_shared_state_a .. d): every copy defines the anchor of the same state type.
// The observer node (the existing GreeterObserverNode) only reports the address of the state the copy resolves to.
// The DLL exports nothing but the bootstrap entry points.

#include "crg/crg_plugin.hpp"
#include "crg/crg.hpp"
#include "greeter_common.hpp"
#include "shared_state_common.hpp"

CRG_DEFINE_PLUGIN(SharedStatePlugin)
CRG_DEFINE_DOMAIN(GreeterDomain)
CRG_DEFINE_NODE_LINK(GreeterObserverNode)
CRG_DEFINE_SHARED_STATE(SharedState)

namespace {

    struct StateObserver final : GreeterObserverNode {
        const char* GetObserverName() const override {
            return reinterpret_cast<const char*>(&::crg::discovery::GetSharedState<SharedState>());
        }
    };
    const StateObserver s_Observer;
}
