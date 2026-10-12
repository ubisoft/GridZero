// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// domain_macros.hpp — user-facing macros for domain and model declaration

#pragma once

#include "crg/models/domain_key.hpp"
#include "crg/discovery/domain_synchronizer.hpp"
#include "crg/discovery/universal_anchor_synchronizer.hpp"
#include "crg/discovery/universal_anchor.hpp"
#include "crg/core/dense_index.hpp"
#include "crg/routing/tensor_arena.hpp"

// Domain identity hashes the full #TDomain text via CRG_DECLARE_HASH (hash.hpp) —
// same full-name basis as CRG_DECLARE_MODEL (model_key.hpp), both normalized
// through RemoveLeadingGlobalNamespace (hash.hpp) for a leading '::'.
#define CRG_DECLARE_CAPABILITY_DOMAIN(TDomain)                                   \
    CRG_DECLARE_HASH(TDomain)                                                    \
    namespace crg::models {                                                      \
        template<>                                                               \
        struct DomainKey<TDomain> : public ::crg::hash::TypeHash<TDomain> {};    \
    }                                                                            \
    CRG_INTERNAL_DECLARE_UNIVERSAL_DOMAIN_ANCHOR(TDomain)                         \
    CRG_DECLARE_NODE_LINK(::crg::models::internal::ModelNodeBase<TDomain>)

#define CRG_DECLARE_DOMAIN(TDomain) CRG_DECLARE_CAPABILITY_DOMAIN(TDomain)

#if CRG_PLUGINS_ENABLED
    #define CRG_INSTANTIATE_CAPABILITY_DOMAIN_ANCHORS(TDomain) \
        template struct ::crg::discovery::internal::SyncAgentHolder<TDomain>; \
        template struct ::crg::discovery::UniversalAnchor<::crg::discovery::DomainArenaPopulator<TDomain>*>; \
        template struct ::crg::discovery::UniversalAnchor<std::vector<const ::crg::discovery::DomainArenaPopulator<TDomain>*>>; \
        template struct ::crg::discovery::internal::DomainAnchorRegistry<TDomain>; \
        CRG_INTERNAL_DEFINE_UNIVERSAL_ANCHOR(::crg::core::internal::DenseIndexState<TDomain>) \
        CRG_DEFINE_NODE_LINK(::crg::models::internal::ModelNodeBase<TDomain>)

    #define CRG_DEFINE_CAPABILITY_DOMAIN(TDomain) \
        namespace crg::discovery { \
            template<> struct DomainSynchronizer<TDomain> { \
                static void Clear() noexcept { \
                    ::crg::discovery::DomainArenaPopulator<TDomain>::Chain::Clear(); \
                } \
                static void OnPluginLoad() noexcept { \
                    ::crg::core::DenseIndexStore<TDomain>::Refresh(); \
                    ::crg::discovery::DomainArenaPopulator<TDomain>::Chain::Build(); \
                } \
                static void OnPluginUnload() noexcept { \
                    ::crg::core::DenseIndexStore<TDomain>::Refresh(); \
                    ::crg::discovery::DomainArenaPopulator<TDomain>::Chain::Build(); \
                } \
            }; \
        } \
        CRG_INSTANTIATE_CAPABILITY_DOMAIN_ANCHORS(TDomain)
#else
    #define CRG_INSTANTIATE_CAPABILITY_DOMAIN_ANCHORS(TDomain)
    #define CRG_DEFINE_CAPABILITY_DOMAIN(TDomain)
#endif

#define CRG_DEFINE_DOMAIN(TDomain) CRG_DEFINE_CAPABILITY_DOMAIN(TDomain)

