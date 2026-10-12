// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

#pragma once
#include "crg/crg.hpp"
#include "crg/discovery/node_link.hpp"

// A chain that the plugins define and the host does not: it can only be shared between the plugins themselves.
struct IPeerNode {
    virtual ~IPeerNode() = default;
    virtual int Id() const = 0;
};

struct PeerNode : ::crg::discovery::NodeLink<PeerNode, IPeerNode> {};

CRG_DECLARE_NODE_LINK(PeerNode)
