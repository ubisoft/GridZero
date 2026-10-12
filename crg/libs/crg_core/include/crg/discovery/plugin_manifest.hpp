// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// plugin_manifest.hpp — macros for defining and bootstrapping a CRG plugin

#pragma once
#include "crg/core/config.hpp"

#if CRG_PLUGINS_ENABLED

#include "crg/host/host_descriptor.hpp"
#include "crg/plugin/bootstrap.hpp"
#include "crg/discovery/sync_node.hpp"
#include "crg/discovery/universal_anchor_synchronizer.hpp"

namespace crg::discovery {

    struct IPluginRecord {
        virtual ~IPluginRecord() = default;
        virtual ::crg::discovery::AnchorLink* GetLocalAnchorsHead() const noexcept = 0;
    };

    struct PluginRecordNode : public NodeLink<PluginRecordNode, IPluginRecord>
    {
        // Replicates the full behavior of CRG_DEFINE_NODE_LINK for the plugin registry
        // using C++17 inline variables, avoiding macro injection in the host.
        struct AnchorBundle {
            ::crg::discovery::UniversalAnchor<PluginRecordNode*> m_PtrAnchor{};
            ::crg::discovery::UniversalAnchor<std::vector<const PluginRecordNode*>> m_VecAnchor{};
            ::crg::discovery::UniversalAnchorSynchronizer<PluginRecordNode*> m_SyncPtrAnchor{};
            ::crg::discovery::UniversalAnchorSynchronizer<std::vector<const PluginRecordNode*>> m_SyncVecAnchor{};
        };

        static AnchorBundle ms_Bundle;

        // Calls fn(TNode&) for the nodes found behind the cell of TNode in the loaded modules, reached from the record
        // chain alone. Each module owns its segment; a chain spliced into the host also yields nodes of other modules
        // behind a segment, so a caller that must not see a node twice dedups.
        template<class TNode, bool TIncludeOwn = false, class TFn>
        static void VisitSegments(TFn&& fn) {
            static constexpr ::crg::u64 CellHash = ::crg::hash::TypeHash<TNode*>::Value;
            const AnchorLink* own = AnchorLink::GetLocalHead();
            Walk(NodeLinkAnchor<PluginRecordNode>::Get(), [own, &fn](const PluginRecordNode& record) {
                AnchorLink* head = record.GetLocalAnchorsHead();
                if (!head || (!TIncludeOwn && head == own)) return;
                Walk(head, [&fn](const AnchorLink& anchor) {
                    if (anchor.GetTypeHash() != CellHash) return;
                    Walk(*static_cast<TNode* const*>(anchor.GetLocalAddress()), fn);
                });
            });
        }

    private:
        // Walks a chain by m_Next from a given head, without the cache; the head may belong to another module.
        template<class TNode, class TFn>
        static void Walk(TNode* head, TFn&& fn) {
            for (TNode* current = head; current; current = current->m_Next) fn(*current);
        }
    };

    inline PluginRecordNode::AnchorBundle PluginRecordNode::ms_Bundle{};

    template<typename TPluginTag>
    class PluginManifest final : public PluginRecordNode {
    public:
        using Splicer = internal::ChainSplicer<PluginRecordNode>;

        ::crg::discovery::AnchorLink* GetLocalAnchorsHead() const noexcept override {
            return ::crg::discovery::AnchorLink::GetLocalHead();
        }
    };

} // namespace crg::discovery

CRG_DECLARE_NODE_LINK(crg::discovery::PluginRecordNode)

#else

namespace crg::discovery {

    // Without plugins there is one module and no record chain.
    struct PluginRecordNode {
        template<class TNode, bool TIncludeOwn = false, class TFn>
        static void VisitSegments(TFn&&) {}
    };

}

#endif
