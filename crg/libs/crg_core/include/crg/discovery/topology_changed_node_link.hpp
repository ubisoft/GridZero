// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// topology_changed_node_link.hpp — NodeLink for plugin topology-change notifications

#pragma once
#include "crg/core/config.hpp"
#include "crg/discovery/node_link.hpp"
#include "crg/discovery/universal_anchor.hpp"
#if CRG_PLUGINS_ENABLED
#include "crg/discovery/plugin_manifest.hpp"
#endif

namespace crg::discovery {

struct ITopologyChanged {
    virtual ~ITopologyChanged() = default;
    virtual void OnTopologyChanged() noexcept = 0;
};

struct TopologyChangedNodeLink
    : NodeLink<TopologyChangedNodeLink, ITopologyChanged> {
#if CRG_PLUGINS_ENABLED
    // Every module that includes this header carries its own anchor cells (inline variable), as PluginRecordNode does:
    // no definition macro in the host or in a package.
    struct AnchorBundle {
        UniversalAnchor<TopologyChangedNodeLink*> m_PtrAnchor{};
        UniversalAnchor<std::vector<const TopologyChangedNodeLink*>> m_VecAnchor{};
        UniversalAnchorSynchronizer<TopologyChangedNodeLink*> m_SyncPtrAnchor{};
        UniversalAnchorSynchronizer<std::vector<const TopologyChangedNodeLink*>> m_SyncVecAnchor{};
    };
    static AnchorBundle ms_Bundle;
#endif
};

#if CRG_PLUGINS_ENABLED
inline TopologyChangedNodeLink::AnchorBundle TopologyChangedNodeLink::ms_Bundle{};
#endif

}

CRG_DECLARE_NODE_LINK(::crg::discovery::TopologyChangedNodeLink)
