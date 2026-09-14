// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// plugin_local_node.hpp — NodeLink variant with plugin-local head traversal

#pragma once
#include "crg/core/config.hpp"
#include "crg/discovery/node_link.hpp"

#if CRG_PLUGINS_ENABLED

namespace crg::discovery {

    template<typename TNode, typename TContract>
    struct PluginLocalNode : NodeLink<TNode, TContract> {

        static TNode* GetLocalHead() noexcept {
            return internal::PluginLocalStorage<TNode>::s_LocalHead;
        }

        template<typename CallableT>
        static void VisitLocal(CallableT&& func) {
            TNode* current = GetLocalHead();
            while (current) {
                TNode* next = current->m_Next;
                func(*current);
                current = next;
            }
        }
    };

}

#endif
