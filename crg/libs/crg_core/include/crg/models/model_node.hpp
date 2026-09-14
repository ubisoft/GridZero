// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// model_node.hpp — intrusive NodeLink node per (Domain, Model) pair

#pragma once
#include "crg/core/config.hpp"
#include "crg/core/types.hpp"
#include "crg/discovery/node_link.hpp"
// A preprocessor directive (#include) cannot be a macro argument — the
// CRG_HASH_NAME_ENABLED_ONLY(...) wrapper used everywhere else in this file
// does not work here; a real #if/#endif is required instead.
#if CRG_HASH_NAME_ENABLED
#include <string_view>
#endif

namespace crg::models::internal {

    struct IModelNode {
        virtual ::crg::u64 GetHash() const noexcept = 0;
        // Available when CRG_HASH_NAME_ENABLED: returns the FULL, untruncated
        // #ModelType string stashed by CRG_DECLARE_MODEL (model_key.hpp) — NOT the
        // truncated leaf that GetHash() is derived from. DenseIndexStore::Refresh
        // compares this to tell a harmless re-registration of the same type apart
        // from a genuine hash collision between two distinct types that share a
        // leaf name; using the leaf here instead would make that comparison
        // trivially pass on a real collision.
        CRG_HASH_NAME_ENABLED_ONLY(virtual std::string_view GetName() const noexcept = 0;)
        virtual ~IModelNode() = default;
    };

    template<class TDomain>
    struct ModelNodeBase : public ::crg::discovery::NodeLink<ModelNodeBase<TDomain>, IModelNode> {};

    template<class TDomain, ::crg::u64 THash>
    struct ModelNode final : public ModelNodeBase<TDomain> {
        ::crg::u64 GetHash() const noexcept final { return THash; }

#if CRG_HASH_NAME_ENABLED
        // When name tracking is active, the node stores a pointer to the type's source name
        // (a static constexpr std::string_view from TypeHashBase<T>, populated by
        // CRG_DECLARE_MODEL).  CRG_DDM_BIND passes &TypeHash<ModelType>::Name at construction.
        const std::string_view* m_Name{ nullptr };
        explicit ModelNode(const std::string_view* name = nullptr) : m_Name(name) {}
        std::string_view GetName() const noexcept final {
            return (m_Name != nullptr) ? *m_Name : std::string_view{};
        }
#endif
    };

} // namespace crg::models::internal
