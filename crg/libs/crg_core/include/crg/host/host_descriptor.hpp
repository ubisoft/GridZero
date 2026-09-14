// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// host_descriptor.hpp — Opaque host memory map passed to dynamic modules

#pragma once
#include "crg/core/types.hpp"
#include "crg/core/hash.hpp"
#include "crg/discovery/universal_anchor.hpp"

#if CRG_PLUGINS_ENABLED

namespace crg::discovery {
    struct AnchorLink;
}

namespace crg::host {

    struct HostDescriptor {
        ::crg::discovery::AnchorLink* m_HostAnchorsHead{ nullptr };

        void* FindAnchorAddressByHash(u64 targetHash) const {
            auto* current = m_HostAnchorsHead;
            while (current) {
                if (current->GetTypeHash() == targetHash) {
                    return current->GetLocalAddress();
                }
                current = current->m_Next;
            }
            return nullptr;
        }

        template<typename T>
        void* GetAnchorAddress() const {
            return FindAnchorAddressByHash(::crg::hash::TypeHash<T>::Value);
        }
    };

} // namespace crg::host

#endif
