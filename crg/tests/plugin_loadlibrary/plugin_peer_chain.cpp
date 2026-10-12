// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// Plugin DLL compiled three times (crg_peer_a .. c, PEER_ID 1, 2, 4): every copy defines the chain of the same node type
// and one node of its own. The host does not define the chain. The DLL reports what its own view of the chain holds.

#include "crg/crg_plugin.hpp"
#include "crg/crg.hpp"
#include "peer_chain_common.hpp"

CRG_DEFINE_PLUGIN(PeerChainPlugin)
CRG_DEFINE_NODE_LINK(PeerNode)

namespace {
    struct PeerMine final : PeerNode {
        int Id() const override { return PEER_ID; }
    };
    const PeerMine s_Mine;
}

extern "C" CRG_EXPORT int CRG_PeerCount() {
    int count = 0;
    PeerNode::Visit([&count](PeerNode&) { ++count; });
    return count;
}

extern "C" CRG_EXPORT int CRG_PeerMask() {
    int mask = 0;
    PeerNode::Visit([&mask](PeerNode& n) { mask |= n.Id(); });
    return mask;
}
