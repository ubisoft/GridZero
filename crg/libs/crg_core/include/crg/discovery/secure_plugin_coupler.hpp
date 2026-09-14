// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// secure_plugin_coupler.hpp — cycle-detection validator for untrusted plugin chains

#pragma once
#include <cstddef>

namespace crg::discovery {

template<typename TNode>
struct SecurePluginCoupler {
    struct Result {
        bool        hasCycle;
        std::size_t nodeCount;  // 0 when hasCycle (walk aborted early)
    };

    static Result Inspect(const TNode* head) noexcept {
        if (!head) return {false, 0};

        const TNode* slow = head;
        const TNode* fast = head;

        while (fast && fast->m_Next) {
            slow = slow->m_Next;
            fast = fast->m_Next->m_Next;
            if (slow == fast) return {true, 0};
        }

        std::size_t count = 0;
        for (const TNode* n = head; n; n = n->m_Next) { ++count; }
        return {false, count};
    }

    static bool IsSafe(const TNode* head) noexcept {
        return !Inspect(head).hasCycle;
    }
};

} // namespace crg::discovery
