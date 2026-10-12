// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// sync_node.hpp — Plugin lifecycle synchronizer node interface

#pragma once
#include "crg/core/config.hpp"
#include "crg/discovery/plugin_local_node.hpp"

#if CRG_PLUGINS_ENABLED
namespace crg::host {
    struct HostDescriptor;
    extern "C" struct CRG_Context;
}
namespace crg::discovery {
    struct ISyncNode;
}

CRG_MARK_INTERFACE_BOOTSTRAP(::crg::discovery::ISyncNode)

namespace crg::discovery {

    struct ISynchronizer {
        virtual ~ISynchronizer() = default;

        // Phase 1: Establish memory bridges for vectors and raw data
        virtual void BindStorage(::crg::host::CRG_Context& ctx) {}

        // Phase 2: Insert nodes into the chain and populate caches
        virtual void SpliceTopology(::crg::host::CRG_Context& ctx) {}

        // Phase 2b (probe): after every module record is visible, link anchors to state owned by other loaded modules
        virtual void LinkStorage(::crg::host::CRG_Context& ctx) {}

        // Phase 2c: before this module leaves, hand its state over to the modules still linked to it
        virtual void UnlinkStorage() {}

        // Phase 3: Purge arena data and sever the chain links
        virtual void UnspliceTopology() {}

        // Phase 4: Reset memory pointers and cut the data bridges
        virtual void UnbindStorage() {}
    };

    struct ISyncNode : PluginLocalNode<ISyncNode, ISynchronizer> {};

}

#endif
