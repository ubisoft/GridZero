// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// capability_router.hpp — domain-scoped capability dispatch engine

#pragma once
#include "crg/core/config.hpp"
#include "crg/core/dense_index.hpp"
#include "crg/core/types.hpp"
#include "crg/models/dense_id.hpp"
#include "crg/models/model_token.hpp"
#include "crg/routing/tensor_arena.hpp"
#include "crg/routing/capability_routing_traits.hpp"
#include "crg/discovery/universal_anchor.hpp"
#include "crg/discovery/arena_populator.hpp"
#include "crg/capabilities/binding_target.hpp"
#include "crg/capabilities/capability_handle.hpp"
#include <cstddef>
#include <type_traits>
#include <utility>

namespace crg::routing {

    template<typename TDomain>
    class CapabilityRouter {
    private:
        template<class T>
        using BindingTarget = ::crg::capabilities::BindingTarget<T>;

        using ModelToken = ::crg::models::ModelToken<TDomain>;

        template<class T>
        using UniversalAnchor = ::crg::discovery::UniversalAnchor<T>;

        template<class TContract>
        using Arena = ::crg::routing::TensorArena<TDomain, TContract>;

        template<class TContract>
        using Traits = ::crg::routing::CapabilityRoutingTraits<TDomain, TContract>;

        template<class TContract>
        using FullContext = ::crg::routing::FullContext<TDomain, TContract>;

		template<class TContract>
		using CapabilityHandle = ::crg::capabilities::CapabilityHandle<TContract>;

        using PopulatorList = ::crg::discovery::DomainArenaPopulator<TDomain>;

        struct ArenaBuilder {
            ArenaBuilder() {
                PopulatorList::Chain::Build();
            }
        };

        static ArenaBuilder& GetArenaBuilder() {
            static ArenaBuilder s_Builder;
            return s_Builder;
        }

        // First Require<TContract>() builds the domain arenas, then (debug builds only) asserts that every
        // cell of THIS contract's arena has a fallback. Require promises the cell exists; Find does not,
        // since a null handle is a legitimate Find result (see Dispatch). In release an unbound cell is
        // a null-handle call.
        template<class TContract>
        struct RequireGate {
            RequireGate() {
                GetArenaBuilder();
                CRG_ASSERT_ENABLED_ONLY(AssertArenaFullyBound();)
            }

            static void AssertArenaFullyBound() {
                const auto* data{ Arena<TContract>::GetData() };
                const auto  size{ Arena<TContract>::GetSize() };
                for (std::size_t i{ 0 }; i < size; ++i) {
                    CRG_ASSERT(data[i].m_Fallback, "CRG Require: a cell of this contract has no binding.");
                }
            }
        };

        template<class TContract>
        static RequireGate<TContract>& GetRequireGate() {
            static RequireGate<TContract> s_Gate;
            return s_Gate;
        }

    public:
        static bool AllPopulatorsResolvable() noexcept {
            bool allResolvable = true;
            PopulatorList::Visit([&allResolvable](::crg::discovery::IArenaPopulator& populator) {
                allResolvable = allResolvable && populator.CanPopulate();
            });
            return allResolvable;
        }

        template<class TContract, typename... TAxes,
            std::enable_if_t<!FullContext<TContract>::RequiresContext, int> = 0>
        static CapabilityHandle<TContract> Find(ModelToken token, TAxes... axes) {
            using Result = CapabilityHandle<TContract>;

            Arena<TContract>::ForceAnchor();
            [[maybe_unused]] ArenaBuilder& triggerBuilder = GetArenaBuilder();

            if (!token.IsValid()) return Result{};

            using TSpace = typename Traits<TContract>::SpaceType;
            const auto* arenaData{ Arena<TContract>::GetData() };
            const auto  arenaSize{ Arena<TContract>::GetSize() };

            // token.GetDenseIndex() is the Dense ID index; Horner's method gives O(1) branchless offset
            std::size_t offset{ TSpace::ComputeOffset(token.GetDenseIndex(), axes...) };

            if (offset >= arenaSize) return Result{};

            const auto& cell{ arenaData[offset] };
            typename FullContext<TContract>::Base ctx{};

            return cell.Resolve(ctx);
        }

        template<class TContract, typename... TAxes,
            std::enable_if_t<FullContext<TContract>::RequiresContext, int> = 0>
        static CapabilityHandle<TContract> Find(
            ModelToken token, const typename FullContext<TContract>::Base& ruleContext, TAxes... axes) {
            using Result = CapabilityHandle<TContract>;

            Arena<TContract>::ForceAnchor();
            [[maybe_unused]] ArenaBuilder& triggerBuilder = GetArenaBuilder();

            if (!token.IsValid()) return Result{};

            using TSpace = typename Traits<TContract>::SpaceType;
            const auto* arenaData{ Arena<TContract>::GetData() };
            const auto  arenaSize{ Arena<TContract>::GetSize() };

            // token.GetDenseIndex() is the Dense ID index; Horner's method gives O(1) branchless offset
            std::size_t offset{ TSpace::ComputeOffset(token.GetDenseIndex(), axes...) };

            if (offset >= arenaSize) return Result{};

            const auto& cell{ arenaData[offset] };

            return cell.Resolve(ruleContext);
        }

        // Require is Find() without the validity checks: it assumes a typed token and in-range enum
        // axes, and asserts both in debug builds only.
        // It assumes a valid token and an in-range cell: both are checked by CRG_ASSERT (debug builds
        // only), so the release hot path is the Horner offset plus one table load. The first call builds
        // the arenas lazily, like Find(), and in debug builds asserts the whole contract arena is bound.
        template<class TContract, typename... TAxes,
            std::enable_if_t<!FullContext<TContract>::RequiresContext, int> = 0>
        static CapabilityHandle<TContract> Require(ModelToken token, TAxes... axes) {
            Arena<TContract>::ForceAnchor();
            [[maybe_unused]] RequireGate<TContract>& triggerGate = GetRequireGate<TContract>();

            CRG_ASSERT(token.IsValid(), "CRG Require: invalid model token.");

            using TSpace = typename Traits<TContract>::SpaceType;
            const std::size_t offset{ TSpace::ComputeOffset(token.GetDenseIndex(), axes...) };

            CRG_ASSERT(offset < Arena<TContract>::GetSize(), "CRG Require: cell offset outside the arena.");

            typename FullContext<TContract>::Base ctx{};
            return Arena<TContract>::GetData()[offset].Resolve(ctx);
        }

        template<class TContract, typename... TAxes,
            std::enable_if_t<FullContext<TContract>::RequiresContext, int> = 0>
        static CapabilityHandle<TContract> Require(
            ModelToken token, const typename FullContext<TContract>::Base& ruleContext, TAxes... axes) {
            Arena<TContract>::ForceAnchor();
            [[maybe_unused]] RequireGate<TContract>& triggerGate = GetRequireGate<TContract>();

            CRG_ASSERT(token.IsValid(), "CRG Require: invalid model token.");

            using TSpace = typename Traits<TContract>::SpaceType;
            const std::size_t offset{ TSpace::ComputeOffset(token.GetDenseIndex(), axes...) };

            CRG_ASSERT(offset < Arena<TContract>::GetSize(), "CRG Require: cell offset outside the arena.");

            return Arena<TContract>::GetData()[offset].Resolve(ruleContext);
        }

        // Dispatch is factored out separately because MSVC fails to deduce (axes..., func)
        // when both are forwarded from a variadic caller in a single template parameter pack.
        template<class TContract, class... TArgs, class TFunc>
        static void Invoke(ModelToken token, TArgs&&... axes, TFunc&& func) {
            static_assert(!::crg::capabilities::IsStaticContract<TContract>,
                "Router::Invoke is for Brain contracts only — DOD hot path: Find() + operator()");
            auto cap = Find<TContract>(token, std::forward<TArgs>(axes)...);
            Dispatch(cap, std::forward<TFunc>(func));
        }

        template<class TContract, class TFunc>
        static void Dispatch(const CapabilityHandle<TContract>& cap, TFunc&& func) {
            static_assert(!::crg::capabilities::IsStaticContract<TContract>,
                "Router::Dispatch is for Brain contracts only — DOD hot path: Find() + operator()");
            if (!cap) return;
            std::forward<TFunc>(func)(*cap);
        }
    };

} // namespace crg::routing
