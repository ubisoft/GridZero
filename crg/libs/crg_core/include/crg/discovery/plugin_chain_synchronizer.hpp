// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// plugin_chain_synchronizer.hpp — ISynchronizer splicing a chain across plugin/host boundary

#pragma once
#include "crg/core/config.hpp"

#if CRG_PLUGINS_ENABLED

#include "crg/discovery/chain_splicer.hpp"
#include "crg/discovery/sync_node.hpp"
#include "crg/host/plugin_context.hpp"

namespace crg::discovery {

    template<class TDomain>
    struct DomainSynchronizer;

    template<class TDomain, template<class> class TNode>
    struct PluginChainSynchronizer final : public ISyncNode {
        using NodeType = TNode<TDomain>;
        using Splicer  = internal::ChainSplicer<NodeType>;

        void OnPluginLoad(::crg::host::CRG_Context& ctx) override {
            if (Splicer::Splice(*ctx.m_HostDescriptor)) {
                DomainSynchronizer<TDomain>::OnPluginLoad();
            }
        }

        void OnPluginUnload() override {
            // The cross-binary redirect stays open across the user hook so
            // any cache writes performed by OnPluginUnload land in the
            // host's storage, not the plugin's. ClosePortal severs the
            // redirect afterwards.
            if (Splicer::Unsplice()) {
                DomainSynchronizer<TDomain>::OnPluginUnload();
            }
            Splicer::ClosePortal();
        }
    };

} // namespace crg::discovery

#endif // CRG_PLUGINS_ENABLED
