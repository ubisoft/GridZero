// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

#pragma once
#include "crg/crg.hpp"
#include "crg/discovery/universal_anchor.hpp"

// A domain the HOST does not define: its chains never join a host chain, so a module sees the other modules' nodes
// only through the plugin record chain (PluginRecordNode::VisitSegments). The host defines only the log below.
struct OrderDomain {};
CRG_DECLARE_DOMAIN(OrderDomain)

struct OrderLog {
    int m_Populates[2]{};
    int m_Seq[16]{};      // populate calls in order: plugin role (0/1) or 2 for the host
    int m_SeqCount{};
};
CRG_DECLARE_SHARED_STATE(OrderLog)

// A domain the HOST defines (host model JoinHost, host populator), so every module's chains join the host's. Two plugins
// (role 0 / role 1) each add one model and one populator.
struct JoinDomain {};
CRG_DECLARE_DOMAIN(JoinDomain)
struct JoinHost {};
