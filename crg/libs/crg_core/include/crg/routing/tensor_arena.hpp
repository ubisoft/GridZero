// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// tensor_arena.hpp - Per-contract dispatch-cell storage arena for capability routing

#pragma once
#include "crg/core/config.hpp"
#include "crg/core/hash.hpp"
#include "crg/discovery/universal_anchor.hpp"
#include "crg/capabilities/binding_target.hpp"
#include "crg/capabilities/capability.hpp"
#include "crg/capabilities/capability_handle.hpp"
#include "crg/routing/capability_routing_traits.hpp"
#include <vector>
#include <limits>

namespace crg::capabilities {
    template<class TDomain, class TModel, template<class...> class... TCapabilities>
    struct CapabilityBinding;

    template<class TDomain, class TModel, template<class...> class TCap, class TIdxSeq>
    struct CapabilityNode;
}

namespace crg::routing {

    template<typename TDomain>
    class CapabilityRouter;

    template<typename TDomain, typename TContract>
    struct DynamicRule {
        using ContextType  = ContextTypeOf<TDomain, TContract>;
        using PredicatePtr = bool (*)(const void*, const ContextType&);
        using Descriptor   = ::crg::capabilities::BindingTarget<TContract>;

        Descriptor   m_Descriptor{};
        const void*  m_ConfigData{ nullptr };
        PredicatePtr m_Predicate{ nullptr };

        bool Matches(const ContextType& ctx) const {
            return !m_Predicate || m_Predicate(m_ConfigData, ctx);
        }
    };

    template<typename TDomain, typename TContract, bool TIsDynamic = HasDynamicRules<TDomain, TContract>>
    struct DispatchCell;

    template<typename TDomain, typename TContract>
    struct DispatchCell<TDomain, TContract, true> {
        using ContextType = ContextTypeOf<TDomain, TContract>;
        using CapHandle   = ::crg::capabilities::CapabilityHandle<TContract>;
        using Target      = ::crg::capabilities::BindingTarget<TContract>;

        std::vector<DynamicRule<TDomain, TContract>> m_DynamicRules{};
        Target m_Fallback{};

        template<class Impl>
        void Bind(const Target& target, const Impl* instance) {
            if constexpr (::crg::capabilities::HasConfigType<Impl>::Value) {
                auto tramp = [](const void* obj, const ContextType& ctx) -> bool {
                    return static_cast<const Impl*>(obj)->m_Config.Condition(ctx);
                };
                m_DynamicRules.push_back({
                    target,
                    static_cast<const void*>(&instance->m_Config),
                    tramp
                });
            } else if constexpr (std::is_base_of_v<TContract, Impl> ||
                                  std::is_base_of_v<::crg::capabilities::Capability<TContract>, Impl>) {
                m_Fallback = target;
            }
        }

        CapHandle Resolve(const ContextType& ctx) const {
            for (const auto& rule : m_DynamicRules) {
                if (rule.Matches(ctx)) {
                    CapHandle r; r = &rule.m_Descriptor; return r;
                }
            }
            CapHandle r; r = &m_Fallback; return r;
        }
    };

    template<typename TDomain, typename TContract>
    struct DispatchCell<TDomain, TContract, false> {
        using ContextType = ContextTypeOf<TDomain, TContract>;
        using CapHandle   = ::crg::capabilities::CapabilityHandle<TContract>;
        using Target      = ::crg::capabilities::BindingTarget<TContract>;

        Target m_Fallback{};

        template<class Impl>
        void Bind(const Target& target, const Impl*) {
            static_assert(!::crg::capabilities::HasConfigType<Impl>::Value,
                "config-bearing capability bound to a DispatchCell with HasDynamicRules = false");
            if constexpr (std::is_base_of_v<TContract, Impl> ||
                          std::is_base_of_v<::crg::capabilities::Capability<TContract>, Impl>) {
                m_Fallback = target;
            }
        }

        CapHandle Resolve(const ContextType&) const {
            CapHandle r; r = &m_Fallback; return r;
        }
    };

    template<typename TDomain, typename TContract, typename = void>
    struct CellSelector {
        using Type = DispatchCell<TDomain, TContract>;
    };

    template<typename TDomain, typename TContract>
    struct CellSelector<TDomain, TContract,
        std::void_t<typename CapabilityRoutingTraits<TDomain, TContract>::DispatchCellType>> {
        using Type = typename CapabilityRoutingTraits<TDomain, TContract>::DispatchCellType;
    };

    template<typename TDomain, typename TContract> class TensorArena;

    template<class TDomain, class TContract>
    struct TensorArenaStorage {
    private:
        std::vector<typename CellSelector<TDomain, TContract>::Type> m_Cells{};
        friend class TensorArena<TDomain, TContract>;
    };

    template<typename TDomain, typename TContract>
    class TensorArena {
    public:
        using Storage  = TensorArenaStorage<TDomain, TContract>;
        using CellType = typename CellSelector<TDomain, TContract>::Type;
        using Anchor   = ::crg::discovery::UniversalAnchor<Storage>;

        template<class TDomainFriend, class TModelFriend, template<class...> class TCapabilitiesFriend, class TIdxSeqFriend>
        friend struct ::crg::capabilities::CapabilityNode;

        template<class TDomainFriend, class TModelFriend, template<class...> class... TCapabilitiesFriend>
        friend struct ::crg::capabilities::CapabilityBinding;

        static const CellType* GetData() { return Get().data(); }
        static std::size_t     GetSize() { return Get().size(); }

    private:
        CRG_PLUGINS_ENABLED_ONLY(inline static Anchor ms_Anchor{};)

        static std::vector<CellType>& Get() { return Anchor::Get().m_Cells; }
    };

}

namespace crg::hash {
    template<class TDomain, class TContract>
    struct TypeHash<::crg::routing::TensorArenaStorage<TDomain, TContract>> {
        static constexpr ::crg::u64 Value =
            TypeHash<TDomain>::Value ^ (TypeHash<TContract>::Value + 0x9e3779b97f4a7c15ULL
                + (TypeHash<TDomain>::Value << 6) + (TypeHash<TDomain>::Value >> 2));
        CRG_HASH_NAME_ENABLED_ONLY(static constexpr std::string_view Name = "TensorArenaStorage";)
    };
}
