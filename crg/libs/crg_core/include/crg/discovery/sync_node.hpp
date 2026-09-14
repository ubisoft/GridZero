// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// sync_node.hpp — Plugin lifecycle synchronizer node interface

#pragma once
#include "crg/core/config.hpp"
#include "crg/discovery/plugin_local_node.hpp"

#if CRG_PLUGINS_ENABLED

#include "crg/discovery/universal_anchor.hpp"

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
        virtual void OnPluginPreLoad(::crg::host::CRG_Context&) {}
        virtual void OnPluginLoad(::crg::host::CRG_Context& ctx) = 0;
        virtual void OnPluginUnload() = 0;
    };

    struct ISyncNode : PluginLocalNode<ISyncNode, ISynchronizer> {};

}

CRG_DECLARE_UNIVERSAL_NODE_ANCHOR(::crg::discovery::ISyncNode)

#endif
