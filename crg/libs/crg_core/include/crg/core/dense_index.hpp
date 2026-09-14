// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// dense_index.hpp — per-domain hash-to-slot read-only directory

#pragma once
#include "crg/core/config.hpp"
#include "crg/core/hash.hpp"
#include "crg/core/types.hpp"
#include "crg/discovery/universal_anchor.hpp"
#include "crg/models/dense_id.hpp"
#include "crg/models/model_node.hpp"
#include <vector>
#include <cstdio>
// A preprocessor directive (#include) cannot be a macro argument — the
// CRG_HASH_NAME_ENABLED_ONLY(...) wrapper used elsewhere in this file does
// not work here; a real #if/#endif is required instead.
#if CRG_HASH_NAME_ENABLED
#include <string_view>
#endif

namespace crg::core {

    // Alias defined here rather than in dense_id.hpp to break the mutual include cycle.
    inline constexpr u32 InvalidDenseSlot = ::crg::models::InvalidModelSlot;

    namespace internal {

        template<class TDomain>
        struct DenseIndexState {
            std::vector<u64> m_Hashes{};
            // When CRG_HASH_NAME_ENABLED, store the source name alongside each hash.
            // Used in debug to detect FNV-1a collisions between distinct type names.
            CRG_HASH_NAME_ENABLED_ONLY(std::vector<std::string_view> m_Names{};)
        };

    } // namespace internal

    template<class TDomain>
    struct DenseIndexStore {
        using State  = internal::DenseIndexState<TDomain>;
        using Anchor = ::crg::discovery::UniversalAnchor<State>;
        using Chain  = ::crg::models::internal::ModelNodeBase<TDomain>;

        static void Refresh() {
            State& state = Anchor::Get();
            Chain::RefreshCache();
            for (const auto* node : Chain::GetCache()) {
                const u64 hash = node->GetHash();
                const u32 existing = FindIn(state, hash);
                if (existing != InvalidDenseSlot) {
                    // A duplicate hash means either:
                    //   (a) the same model type registered twice — harmless dedup, or
                    //   (b) two DISTINCT model types whose truncated leaf name collides
                    //       (e.g. studio::Scout and mod::Scout both hash to "Scout") — a
                    //       real identity clash that would otherwise silently merge two
                    //       models into one slot.
                    // Names here are the FULL #ModelType strings (see CRG_DECLARE_MODEL,
                    // model_key.hpp) — not the leaf the hash is derived from — precisely so
                    // (a) and (b) are distinguishable: same full name -> (a), different full
                    // name -> (b). This only catches the co-compiled case (debug-only); a
                    // clash between independently-shipped plugins/language guests needs a
                    // load-time check instead, since only the hash crosses that boundary.
                    // assert() takes a static string, so the two colliding full names can't
                    // be embedded in its message — print them to stderr right before the
                    // assert fires, and only then, so harmless dedup (the common case) stays
                    // silent.
                    CRG_HASH_NAME_ENABLED_ONLY(
                        if (state.m_Names[existing] != node->GetName()) {
                            std::fprintf(stderr,
                                "[CRG] model identity collision on hash=%llx: existing='%.*s' vs new='%.*s'\n",
                                (unsigned long long)hash,
                                (int)state.m_Names[existing].size(), state.m_Names[existing].data(),
                                (int)node->GetName().size(), node->GetName().data());
                        }
                        CRG_ASSERT(state.m_Names[existing] == node->GetName(),
                            "CRG: model identity collision — two distinct model types share "
                            "the same leaf-truncated hash. Rename one of the colliding types, "
                            "move one to a different domain, or provide an explicit hash "
                            "override. See the stderr line just printed for the two full "
                            "colliding type names.");
                    )
                    continue;
                }
                state.m_Hashes.push_back(hash);
                CRG_HASH_NAME_ENABLED_ONLY(state.m_Names.push_back(node->GetName());)
            }
        }

        static u32 Find(u64 hash) noexcept {
            [[maybe_unused]] const Bootstrap& trigger = GetBootstrap();
            return FindIn(Anchor::Get(), hash);
        }

        static u32 Size() noexcept {
            [[maybe_unused]] const Bootstrap& trigger = GetBootstrap();
            return static_cast<u32>(Anchor::Get().m_Hashes.size());
        }

    private:
        struct Bootstrap {
            Bootstrap() { Refresh(); }
        };

        static const Bootstrap& GetBootstrap() noexcept {
            static const Bootstrap s_Bootstrap;
            return s_Bootstrap;
        }

        static u32 FindIn(const State& state, u64 hash) noexcept {
            const std::size_t n = state.m_Hashes.size();
            for (std::size_t i = 0; i < n; ++i) {
                if (state.m_Hashes[i] == hash) return static_cast<u32>(i);
            }
            return InvalidDenseSlot;
        }
    };

} // namespace crg::core

namespace crg::hash {
    template<class TDomain>
    struct TypeHash<::crg::core::internal::DenseIndexState<TDomain>> {
        static constexpr ::crg::u64 Value =
            TypeHash<TDomain>::Value ^ 0x5a3c75939a3779b9ULL;
        CRG_HASH_NAME_ENABLED_ONLY(static constexpr std::string_view Name = "DenseIndexState";)
    };
}
