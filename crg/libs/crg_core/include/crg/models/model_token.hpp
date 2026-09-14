// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// model_token.hpp — Runtime-resolved model identity consumed by CapabilityRouter

#pragma once
#include "crg/core/dense_index.hpp"
#include "crg/models/dense_id.hpp"
#include "crg/models/domain_traits.hpp"
#include <type_traits>

namespace crg::routing { template<typename TDomain> class CapabilityRouter; }

namespace crg::models {

    template<typename TDomain>
    struct ModelToken {
    private:
        using DenseID    = DenseModelID<TDomain>;
        using DenseIndex = ::crg::core::DenseIndexStore<TDomain>;

        // Direct slot construction, bypassing the store lookup. Reserved for
        // CapabilityRouter's own ValidateContract(), which must re-derive a
        // token for every already-resolved slot in the store — never exposed
        // for test/benchmark convenience.
        explicit ModelToken(DenseID slot) : m_Slot(slot) {}
        friend class ::crg::routing::CapabilityRouter<TDomain>;

    public:
        ModelToken() = default;

        // Resolved dense slot — consumed by the router's branchless offset
        // arithmetic, and by any code that needs the raw slot value (e.g.
        // comparing against a foreign u64 identifier). Callers that only
        // need identity comparison should prefer operator== instead.
        inline u32 GetDenseIndex() const {
            return m_Slot.m_Value;
        }

        inline bool IsValid() const {
            return m_Slot.IsValid();
        }

        inline bool operator==(const ModelToken& other) const {
            return m_Slot == other.m_Slot;
        }

        inline bool operator!=(const ModelToken& other) const {
            return !(*this == other);
        }

        // COLD PATH — pure Find against the populated store, never allocates.
        void Set(u64 hash) {
            m_Slot = DenseID{ DenseIndex::Find(hash) };
        }

        // static_assert via DomainTraits::TypeKey covers declaration only —
        // not bootstrap timing. The slot is still resolved at runtime against
        // the populated store.
        template<typename TModel>
        static ModelToken FromType() {
            constexpr u64 key = DomainTraits<TDomain>::template TypeKey<std::decay_t<TModel>>;
            return FromKey(key);
        }

        static ModelToken FromKey(u64 key) {
            ModelToken token;
            token.Set(key);
            return token;
        }

        static ModelToken FromHash(u64 hash) {
            return FromKey(hash);
        }

    private:
        DenseID m_Slot{};
    };

} // namespace crg::models
