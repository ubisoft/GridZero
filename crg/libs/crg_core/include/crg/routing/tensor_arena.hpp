// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// tensor_arena.hpp - Per-contract dispatch-cell storage arena for capability routing

#pragma once
#include "crg/core/config.hpp"
#include "crg/core/hash.hpp"
#include "crg/discovery/universal_anchor_synchronizer.hpp"
#include "crg/discovery/plugin_manifest.hpp"
#include "crg/capabilities/binding_target.hpp"
#include "crg/capabilities/capability.hpp"
#include "crg/capabilities/capability_handle.hpp"
#include "crg/routing/capability_routing_traits.hpp"
#include "crg/routing/dynamic_rule_traits.hpp"
#include <vector>
#include <limits>

#include "crg/plugin/plugin.hpp"
#include "crg/host/host_descriptor.hpp"

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
    class DynamicRule {
    public:
        using ContextType  = ContextTypeOf<TDomain, TContract>;
        using PredicatePtr = bool (*)(const void*, const ContextType&);
        using Descriptor   = ::crg::capabilities::BindingTarget<TContract>;

        Descriptor m_Descriptor{};
        const void*  m_ConfigData{ nullptr };
        PredicatePtr m_Predicate{ nullptr };

    public:
        DynamicRule() = default;
        
        DynamicRule(const Descriptor& desc, const void* cfg, PredicatePtr pred)
            : m_Descriptor(desc), m_ConfigData(cfg), m_Predicate(pred) {}

        // Method is completely removed from the overload set if TConfig is void.
        template<
            typename TConfig = typename DynamicRuleTraits<TDomain, TContract>::ConfigBaseType,
            std::enable_if_t<!std::is_same_v<TConfig, void>, int> = 0
        >
        const TConfig* GetConfig() const noexcept {
            return static_cast<const TConfig*>(m_ConfigData);
        }

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
    template<typename TDomain, typename TContract> class HostStitcher;

    template<class TDomain, class TContract>
    struct TensorArenaStorage {
    private:
        std::vector<typename CellSelector<TDomain, TContract>::Type> m_Cells{};
        friend class TensorArena<TDomain, TContract>;
        friend class HostStitcher<TDomain, TContract>;
    };

    namespace details {
        // Primary template: assumes the member does not exist.
        template <typename T, typename = void>
        struct HasDynamicRulesMember : std::false_type {};

        // Specialization: compiler selects this if T::m_DynamicRules is valid.
        template <typename T>
        struct HasDynamicRulesMember<T, std::void_t<decltype(std::declval<T&>().m_DynamicRules)>> : std::true_type {};
    }

    template<typename TDomain, typename TContract>
    class TensorArena {
    public:
        using Storage  = TensorArenaStorage<TDomain, TContract>;
        using CellType = typename CellSelector<TDomain, TContract>::Type;
        using Anchor   = ::crg::discovery::UniversalAnchor<Storage>;
        CRG_PLUGINS_ENABLED_ONLY(using SyncNode = ::crg::discovery::UniversalAnchorSynchronizer<Storage>;)

        template<class TDomainFriend, class TModelFriend, template<class...> class TCapabilitiesFriend, class TIdxSeqFriend>
        friend struct ::crg::capabilities::CapabilityNode;

        template<class TDomainFriend, class TModelFriend, template<class...> class... TCapabilitiesFriend>
        friend struct ::crg::capabilities::CapabilityBinding;

    private:
        CRG_PLUGINS_ENABLED_ONLY(inline static Anchor ms_Anchor{};)
        CRG_PLUGINS_ENABLED_ONLY(inline static SyncNode ms_SyncNode{};)
    
    public:
        static void ForceAnchor() {
            CRG_PLUGINS_ENABLED_ONLY(
                const void* anchorTrigger = &ms_Anchor; (void)anchorTrigger;
                const void* syncTrigger = &ms_SyncNode; (void)syncTrigger;
            )
        }

        static const CellType* GetData() { return Get().data(); }
        static std::size_t     GetSize() { return Get().size(); }

        static void SortCells() {
            // Only attempt to sort if the cell explicitly declares the member.
            if constexpr (details::HasDynamicRulesMember<CellType>::value) {
                for (auto& cell : Get()) {
                    DynamicRuleTraits<TDomain, TContract>::SortRules(cell.m_DynamicRules);
                }
            }
        }

    private:
        static std::vector<CellType>& Get() { ForceAnchor(); return Anchor::Get().m_Cells; }
    };

} // namespace crg::routing

namespace crg::hash {
    template<class TDomain, class TContract>
    struct TypeHash<::crg::routing::TensorArenaStorage<TDomain, TContract>> {
        static constexpr ::crg::u64 Value =
            TypeHash<TDomain>::Value ^ (TypeHash<TContract>::Value + 0x9e3779b97f4a7c15ULL
                + (TypeHash<TDomain>::Value << 6) + (TypeHash<TDomain>::Value >> 2));
        CRG_HASH_NAME_ENABLED_ONLY(static constexpr std::string_view Name = "TensorArenaStorage";)
    };
}
