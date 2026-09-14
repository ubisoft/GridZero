// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// topology_changed_node_link.hpp — NodeLink for plugin topology-change notifications

#pragma once
#include "crg/discovery/node_link.hpp"
#include "crg/discovery/universal_anchor.hpp"

namespace crg::discovery {

struct ITopologyChanged {
    virtual ~ITopologyChanged() = default;
    virtual void OnTopologyChanged() noexcept = 0;
};

struct TopologyChangedNodeLink
    : NodeLink<TopologyChangedNodeLink, ITopologyChanged> {};

}

CRG_DECLARE_UNIVERSAL_NODE_ANCHOR(::crg::discovery::TopologyChangedNodeLink)
