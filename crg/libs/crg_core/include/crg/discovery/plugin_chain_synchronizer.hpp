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

        void BindStorage(::crg::host::CRG_Context& ctx) override {
            // Nothing to do here: memory anchors are handled by UniversalAnchorSynchronizer
        }

        void SpliceTopology(::crg::host::CRG_Context& ctx) override {
            // Phase 2: Topology splicing and cache rebuild 
            // Executed after memory anchors are securely established
            if (Splicer::Splice(*ctx.m_HostDescriptor)) {
                DomainSynchronizer<TDomain>::OnPluginLoad();
            }
        }

        // A module whose chain is not spliced into a host chain still changes what every loaded module can see: rebuild
        // once the storage is linked, so the domain reflects it without waiting for a first use.
        void LinkStorage(::crg::host::CRG_Context& ctx) override {
            if (!Splicer::IsJoined()) {
                DomainSynchronizer<TDomain>::OnPluginLoad();
            }
        }

        void UnspliceTopology() override {
            // Phase 3: Cleanup and topology unsplicing 
            // Executed while memory anchors are still active
            if (Splicer::CanUnsplice()) {
                DomainSynchronizer<TDomain>::Clear();
                Splicer::Unsplice();
                DomainSynchronizer<TDomain>::OnPluginUnload();
                Splicer::ClosePortal();
                return;
            }

            // Not spliced: close this module's own chain first, so the rebuild no longer reaches its nodes.
            // IsJoined() is false here (a joined chain takes the branch above), and a failed Splice() may still have
            // left the redirect set, so the rebuild must run regardless of IsLinkedToHost().
            Splicer::ClosePortal();
            DomainSynchronizer<TDomain>::OnPluginUnload();
        }

        void UnbindStorage() override {
            // Nothing to do here: anchor severing is handled by UniversalAnchorSynchronizer
        }
    };

} // namespace crg::discovery

#endif // CRG_PLUGINS_ENABLED
