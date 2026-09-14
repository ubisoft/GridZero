// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// universal_anchor.hpp — stable per-type memory anchor for cross-binary registry sharing

#pragma once
#include "crg/core/config.hpp"
#include "crg/core/types.hpp"
#include "crg/core/hash.hpp"

namespace crg::discovery {

    template<class T>
    struct UniversalAnchor;

}

#if CRG_PLUGINS_ENABLED
    #include "crg/discovery/universal_anchor_plugins.inl"

    #define CRG_ANCHOR_PASTE_2(a, b) a##b
    #define CRG_ANCHOR_PASTE(a, b)   CRG_ANCHOR_PASTE_2(a, b)

    // The static instance is mandatory: it triggers NodeLink::NodeLink() so the anchor
    // self-registers into PluginLocalStorage<AnchorLink>::s_LocalHead.  Without it the
    // AnchorLink chain is empty and Mass Transfer is a silent no-op.
    #define CRG_DEFINE_UNIVERSAL_ANCHOR(...) \
        template struct ::crg::discovery::UniversalAnchor<__VA_ARGS__>; \
        namespace { \
            [[maybe_unused]] static const ::crg::discovery::UniversalAnchor<__VA_ARGS__> \
                CRG_ANCHOR_PASTE(s_crg_anchor_registration_, __COUNTER__) {}; \
        }
#else
    #include "crg/discovery/universal_anchor_monolith.inl"

    #define CRG_DEFINE_UNIVERSAL_ANCHOR(T)
#endif

#define CRG_DECLARE_UNIVERSAL_NODE_ANCHOR(T)                              \
    CRG_INTERNAL_DECLARE_HASH_CUSTOM(T*, #T "*")                          \
    CRG_INTERNAL_DECLARE_HASH_CUSTOM(std::vector<const T*>, "vec_" #T)

#define CRG_INTERNAL_DECLARE_UNIVERSAL_DOMAIN_ANCHOR(TDomain)       \
    CRG_DECLARE_UNIVERSAL_NODE_ANCHOR(::crg::discovery::DomainArenaPopulator<TDomain>)
