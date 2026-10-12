// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// =============================================================================
// CAPABILITY ROUTING GRID (CRG) - UNIVERSAL ANCHOR (DIAGNOSTIC TRACKING MODE)
// =============================================================================

#pragma once
#include "crg/core/types.hpp"
#include "crg/core/config.hpp"
#include "crg/core/hash.hpp"
#include "crg/discovery/node_link.hpp"
#include <type_traits>

namespace crg::discovery {
    struct AnchorLink;
}

// Register ISyncNode inside the DLL local execution pipeline
CRG_MARK_INTERFACE_BOOTSTRAP(::crg::discovery::AnchorLink)

namespace crg::discovery {

    namespace internal {
        template<typename T, typename = std::void_t<>>
        struct IsChainNode : std::false_type {};

        template<typename T>
        struct IsChainNode<T, std::void_t<decltype(std::declval<T&>().m_Next), decltype(T::RefreshCache())>> : std::true_type {};
    }

    struct IAnchor {
        virtual ~IAnchor() = default;
        virtual u64   GetTypeHash() const = 0;
        CRG_HASH_NAME_ENABLED_ONLY(virtual std::string_view GetTypeName() const = 0);
        virtual void* GetLocalAddress() const = 0;
        // Links an unlinked anchor to the state of the same type owned by another module. No effect when already linked.
        virtual void  Link(void* target) = 0;
        // Drops the link: the module uses its own instance again.
        virtual void  Unlink() = 0;
    };

    struct AnchorLink : public NodeLink<AnchorLink, IAnchor>
    {
        CRG_PLUGINS_ENABLED_ONLY(static AnchorLink* GetLocalHead() { return internal::PluginLocalStorage<AnchorLink>::s_LocalHead; })
    };

    // =============================================================================
    // 1. MASTER IMPLEMENTATION WITH LIVE MEMORY STREAMING
    // =============================================================================
    template<typename T>
    struct UniversalAnchor : public AnchorLink {
        static inline T* s_RemoteTarget{ nullptr };

        template<typename THostDescriptor>
        static void SetRemoteTarget(const THostDescriptor& hostDesc) {
            s_RemoteTarget = static_cast<T*>(hostDesc.template GetAnchorAddress<T>());
        }

        static void SetRemoteTarget(std::nullptr_t) {
            s_RemoteTarget = nullptr;
        }


        static void LinkTo(void* target) { if (!s_RemoteTarget) s_RemoteTarget = static_cast<T*>(target); }

        static bool IsLinkedToHost() noexcept {
            return s_RemoteTarget != nullptr;
        }

        static T& GetLocalInstance() {
            static T s_LocalInstance{};
            return s_LocalInstance;
        }

        static T& GetSavedRemoteTarget() {
            static T s_SavedRemoteTarget{};
            return s_SavedRemoteTarget;
        }

        static T& GetSavedLocalTail() {
            static T s_SavedLocalTail{};
            return s_SavedLocalTail;
        }

        // A node chain joined through another module's cell is cut out before the link is dropped: the segment of this
        // module leaves the shared chain, and the module sees only its own nodes again.
        static void DetachSegment() {
            if constexpr (std::is_pointer_v<T>) {
                using TNode = std::remove_pointer_t<T>;
                if constexpr (internal::IsChainNode<TNode>::value) {
                    T localHead = GetLocalInstance();
                    T localTail = GetSavedLocalTail();
                    if (s_RemoteTarget && localHead && localTail) {
                        T afterUs = localTail->m_Next;
                        T& sharedHead = *s_RemoteTarget;
                        if (sharedHead == localHead) {
                            sharedHead = afterUs;
                        } else if (sharedHead) {
                            T node = sharedHead;
                            while (node && node->m_Next != localHead) node = node->m_Next;
                            if (node) node->m_Next = afterUs;
                        }
                        localTail->m_Next = nullptr;
                        GetSavedLocalTail() = nullptr;
                        GetSavedRemoteTarget() = nullptr;
                        s_RemoteTarget = nullptr;
                        UniversalAnchor<std::vector<const TNode*>>::s_RemoteTarget = nullptr;
                        TNode::RefreshCache();
                    }
                }
            }
        }

        static T& Get() {
            static_assert(sizeof(::crg::hash::TypeHash<T>) > 0,
                "CRG Error: Type not registered. Ensure CRG_DECLARE_DOMAIN, CRG_DECLARE_MODEL, or CRG_DECLARE_CONTRACT is used.");

            if (s_RemoteTarget) {
                return *s_RemoteTarget;
            }
            return GetLocalInstance();
        }

        CRG_HASH_NAME_ENABLED_ONLY(std::string_view GetTypeName() const override { return ::crg::hash::TypeHash<T>::Name; });
        u64   GetTypeHash()       const override { return ::crg::hash::TypeHash<T>::Value; }
        void* GetLocalAddress()   const override { return (void*)&GetLocalInstance(); }
        void  Link(void* target) override { LinkTo(target); }
        void  Unlink() override { DetachSegment(); s_RemoteTarget = nullptr; }
    };

    // =============================================================================
    // 2. THE ZERO-CODE REDIRECTION BRIDGE
    // =============================================================================
    template<typename T>
    struct UniversalAnchor<const T*> : public UniversalAnchor<T*> {
        // Inherits everything, routing calls transparently to the base tracker
    };

} // namespace crg::discovery
