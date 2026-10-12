// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// universal_anchor_synchronizer.hpp — Lifecycle synchronizer for both raw anchors and node chains

#pragma once
#include "crg/core/config.hpp"

#if CRG_PLUGINS_ENABLED

#include "crg/discovery/chain_splicer.hpp"
#include "crg/discovery/sync_node.hpp"
#include "crg/host/plugin_context.hpp"
#include "crg/discovery/universal_anchor.hpp"
#include "crg/discovery/node_link.hpp"
#include "crg/host/host_descriptor.hpp"
#include <type_traits>


namespace crg::discovery::internal {
    template<typename TNode, typename TContract>
    TContract extract_contract_helper(const NodeLink<TNode, TContract>*);

    template<typename T, typename = std::void_t<>>
    struct IsNodeLinkDerived : std::false_type {};

    template<typename T>
    struct IsNodeLinkDerived<T, std::void_t<decltype(extract_contract_helper(std::declval<T*>()))>> : std::true_type {};
}

namespace crg::discovery {
    struct PluginRecordNode;

    template<class T>
    struct UniversalAnchorSynchronizer : public ISyncNode {
        // Strip the pointer (if any) to expose the raw node type for traits evaluation
        using BaseType = std::remove_pointer_t<T>;

        void BindStorage(::crg::host::CRG_Context& ctx) override {
            const ::crg::host::HostDescriptor& hostDesc = *ctx.m_HostDescriptor;
            
            // Check inheritance using the stripped type
            if constexpr (!internal::IsNodeLinkDerived<BaseType>::value) {
                // Keep the original T (e.g. T* or std::vector) for the anchor target
                UniversalAnchor<T>::SetRemoteTarget(hostDesc);
            }
        }

        void SpliceTopology(::crg::host::CRG_Context& ctx) override {
            const ::crg::host::HostDescriptor& hostDesc = *ctx.m_HostDescriptor;
            
            // Check inheritance using the stripped type
            if constexpr (internal::IsNodeLinkDerived<BaseType>::value) {
                using Splicer = internal::ChainSplicer<BaseType>;
                
                if (Splicer::Splice(hostDesc)) {
                    BaseType::RefreshCache();
                }
            }
        }

        void UnspliceTopology() override {
            if constexpr (internal::IsNodeLinkDerived<BaseType>::value) {
                using Splicer = internal::ChainSplicer<BaseType>;
                
                if (Splicer::CanUnsplice()) {
                    Splicer::Unsplice();
                    BaseType::RefreshCache();
                }
                Splicer::ClosePortal();
            }
        }

        // Once every record is spliced, link to the state of the same type defined by another loaded module. The record
        // chain is newest first and the last match is the oldest module: it owns the state, because a module only ever
        // links to an older one. Stopping at the first match would link to a module that itself resolves elsewhere.
        template<class TRecord = ::crg::discovery::PluginRecordNode>
        static void LinkFromLoaded() {
            const u64 hash = ::crg::hash::TypeHash<T>::Value;
            AnchorLink* own = AnchorLink::GetLocalHead();
            void* target = nullptr;
            TRecord::Visit([hash, own, &target](TRecord& n) -> bool {
                AnchorLink* head = n.GetLocalAnchorsHead();
                if (!head || head == own) return true;
                for (auto* o = head; o; o = o->m_Next)
                    if (o->GetTypeHash() == hash) { target = o->GetLocalAddress(); break; }
                return true;
            });
            if (target) UniversalAnchor<T>::LinkTo(target);
        }

        template<class TRecord = ::crg::discovery::PluginRecordNode>
        static void* OldestCell(u64 hash) {
            AnchorLink* own = AnchorLink::GetLocalHead();
            void* target = nullptr;
            TRecord::Visit([hash, own, &target](TRecord& n) -> bool {
                AnchorLink* head = n.GetLocalAnchorsHead();
                if (!head || head == own) return true;
                for (auto* o = head; o; o = o->m_Next)
                    if (o->GetTypeHash() == hash) { target = o->GetLocalAddress(); break; }
                return true;
            });
            return target;
        }

        // A chain that the host does not define is shared between the plugins that define it: a module joins the chain
        // held by the oldest loaded module, through the same cell redirect the host chain uses.
        void LinkStorage(::crg::host::CRG_Context& ctx) override {
            if constexpr (!internal::IsNodeLinkDerived<BaseType>::value) {
                if (!UniversalAnchor<T>::IsLinkedToHost()) LinkFromLoaded();
            } else {
                using Splicer = internal::ChainSplicer<BaseType>;
                if (Splicer::IsJoined()) return;
                void* headCell  = OldestCell(::crg::hash::TypeHash<T>::Value);
                void* cacheCell = OldestCell(::crg::hash::TypeHash<std::vector<const BaseType*>>::Value);
                if (!headCell || !cacheCell) return;
                NodeLinkAnchor<BaseType>::LinkTo(headCell);
                CacheAnchor<BaseType>::LinkTo(cacheCell);
                if (Splicer::SpliceLinked()) BaseType::RefreshCache();
            }
        }

        // The owner of a state leaves: every other module that defines the same state goes back to its own instance, so no
        // module keeps an address inside the leaving one.
        template<class TRecord = ::crg::discovery::PluginRecordNode>
        static void UnlinkDependents() {
            const u64 hash = ::crg::hash::TypeHash<T>::Value;
            AnchorLink* own = AnchorLink::GetLocalHead();
            TRecord::Visit([hash, own](TRecord& n) {
                AnchorLink* head = n.GetLocalAnchorsHead();
                if (!head || head == own) return;
                for (auto* o = head; o; o = o->m_Next)
                    if (o->GetTypeHash() == hash) o->Unlink();
            });
        }

        void UnlinkStorage() override {
            if (!UniversalAnchor<T>::IsLinkedToHost()) UnlinkDependents();
        }

        void UnbindStorage() override {
            if constexpr (!internal::IsNodeLinkDerived<BaseType>::value) {
                UniversalAnchor<T>::SetRemoteTarget(nullptr);
            }
        }
    };

} // namespace crg::discovery

#endif // CRG_PLUGINS_ENABLED