// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// domain_macros.hpp — user-facing macros for domain and model declaration

#pragma once

#include "crg/models/domain_key.hpp"
#include "crg/discovery/domain_synchronizer.hpp"
#include "crg/discovery/universal_anchor.hpp"

#include "crg/core/dense_index.hpp"
#include "crg/routing/tensor_arena.hpp"

// Domain identity hashes the full #TDomain text via CRG_DECLARE_HASH (hash.hpp) —
// contrast with CRG_DECLARE_MODEL's leaf-truncated hash (model_key.hpp).
#define CRG_DECLARE_CAPABILITY_DOMAIN(TDomain)                                   \
    CRG_DECLARE_HASH(TDomain)                                                    \
    namespace crg::models {                                                      \
        template<>                                                               \
        struct DomainKey<TDomain> : public ::crg::hash::TypeHash<TDomain> {};    \
    }                                                                            \
    CRG_INTERNAL_DECLARE_UNIVERSAL_DOMAIN_ANCHOR(TDomain)                         \
    CRG_DECLARE_UNIVERSAL_NODE_ANCHOR(::crg::models::internal::ModelNodeBase<TDomain>)

#define CRG_DECLARE_DOMAIN(TDomain) CRG_DECLARE_CAPABILITY_DOMAIN(TDomain)

#if CRG_PLUGINS_ENABLED
    #define CRG_INSTANTIATE_CAPABILITY_DOMAIN_ANCHORS(TDomain) \
        template struct ::crg::discovery::internal::SyncAgentHolder<TDomain>; \
        template struct ::crg::discovery::UniversalAnchor<::crg::discovery::DomainArenaPopulator<TDomain>*>; \
        template struct ::crg::discovery::UniversalAnchor<std::vector<const ::crg::discovery::DomainArenaPopulator<TDomain>*>>; \
        template struct ::crg::discovery::internal::DomainAnchorRegistry<TDomain>;

    #define CRG_DEFINE_CAPABILITY_DOMAIN(TDomain) \
        namespace crg::discovery { \
            template<> struct DomainSynchronizer<TDomain> { \
                static void OnPluginLoad() noexcept { \
                    ::crg::core::DenseIndexStore<TDomain>::Refresh(); \
                    ::crg::discovery::DomainArenaPopulator<TDomain>::RefreshCache(); \
                    ::crg::routing::CapabilityRouter<TDomain>::RefreshCache(); \
                } \
                static void OnPluginUnload() noexcept { \
                    ::crg::core::DenseIndexStore<TDomain>::Refresh(); \
                    ::crg::discovery::DomainArenaPopulator<TDomain>::RefreshCache(); \
                    ::crg::routing::CapabilityRouter<TDomain>::RefreshCache(); \
                } \
            }; \
        } \
        CRG_INSTANTIATE_CAPABILITY_DOMAIN_ANCHORS(TDomain)
#else
    #define CRG_INSTANTIATE_CAPABILITY_DOMAIN_ANCHORS(TDomain)
    #define CRG_DEFINE_CAPABILITY_DOMAIN(TDomain)
#endif

#define CRG_DEFINE_DOMAIN(TDomain) CRG_DEFINE_CAPABILITY_DOMAIN(TDomain)

