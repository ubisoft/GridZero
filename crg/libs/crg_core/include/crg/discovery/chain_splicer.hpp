// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// chain_splicer.hpp — intrusive-chain splice/unsplice for plugin nodes

#pragma once
#include "crg/core/config.hpp"

#if CRG_PLUGINS_ENABLED

#include "crg/discovery/node_link.hpp"
#include "crg/discovery/secure_plugin_coupler.hpp"
#include "crg/discovery/universal_anchor.hpp"

namespace crg::discovery::internal {

template<typename TNode>
struct ChainSplicer {
private:
    using Anchor = NodeLinkAnchor<TNode>;
    using Cache  = CacheAnchor<TNode>;

public:
    template<typename THostDescriptor>
    static bool Splice(const THostDescriptor& hostDesc) noexcept {
        TNode* localHead = Anchor::GetLocalInstance();

        if (!SecurePluginCoupler<TNode>::IsSafe(localHead)) {
            return false;
        }

        Anchor::SetRemoteTarget(hostDesc);
        Cache::SetRemoteTarget(hostDesc);

        if (!localHead || !Anchor::IsLinkedToHost()) {
            return false;
        }

        TNode*& sharedHead = Anchor::Get();
        TNode*  remoteHead = sharedHead;

        if (localHead == remoteHead) {
            return false;
        }

        TNode* tail = localHead;
        while (tail->m_Next) { tail = tail->m_Next; }
        tail->m_Next = remoteHead;
        sharedHead   = localHead;

        Anchor::Get()                  = localHead;
        Anchor::GetSavedRemoteTarget() = remoteHead;
        Anchor::GetSavedLocalTail()    = tail;

        return true;
    }

    static bool Unsplice() noexcept {
        TNode* localHead = Anchor::GetLocalInstance();
        TNode* localTail = Anchor::GetSavedLocalTail();

        if (!localHead || !Anchor::IsLinkedToHost()) {
            return false;
        }

        // localTail->m_Next is the live "after-our-segment" pointer — correct
        // even if another plugin was removed from between us and remoteHead.
        TNode* afterUs = localTail ? localTail->m_Next : nullptr;

        TNode*& sharedHead = Anchor::Get();

        if (sharedHead == localHead) {
            sharedHead = afterUs;
        } else if (sharedHead) {
            TNode* node = sharedHead;
            while (node && node->m_Next != localHead) {
                node = node->m_Next;
            }
            if (node) { node->m_Next = afterUs; }
        }

        if (localTail) { localTail->m_Next = nullptr; }

        return true;
    }

    static void ClosePortal() noexcept {
        Anchor::SetRemoteTarget(nullptr);
        Cache::SetRemoteTarget(nullptr);

        Anchor::GetLocalInstance()     = nullptr;
        Anchor::GetSavedRemoteTarget() = nullptr;
        Anchor::GetSavedLocalTail()    = nullptr;
    }
};

} // namespace crg::discovery::internal

#endif // CRG_PLUGINS_ENABLED
