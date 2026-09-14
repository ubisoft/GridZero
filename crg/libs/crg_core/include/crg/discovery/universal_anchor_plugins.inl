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

    struct IAnchor {
        virtual ~IAnchor() = default;
        virtual u64   GetTypeHash() const = 0;
        virtual void* GetLocalAddress() const = 0;
        virtual void  SetRemoteAddress(void* remoteAddress) = 0;
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

        static T& Get() {
            static_assert(sizeof(::crg::hash::TypeHash<T>) > 0,
                "CRG Error: Type not registered. Ensure CRG_DECLARE_DOMAIN, CRG_DECLARE_MODEL, or CRG_DECLARE_CONTRACT is used.");

            if (s_RemoteTarget) {
                return *s_RemoteTarget;
            }
            return GetLocalInstance();
        }

        u64   GetTypeHash()       const override { return ::crg::hash::TypeHash<T>::Value; }
        void* GetLocalAddress()   const override { return (void*)&GetLocalInstance(); }
        void  SetRemoteAddress(void* remoteAddress) override { s_RemoteTarget = static_cast<T*>(remoteAddress); }
    };

    // =============================================================================
    // 2. THE ZERO-CODE REDIRECTION BRIDGE
    // =============================================================================
    template<typename T>
    struct UniversalAnchor<const T*> : public UniversalAnchor<T*> {
        // Inherits everything, routing calls transparently to the base tracker
    };

} // namespace crg::discovery
