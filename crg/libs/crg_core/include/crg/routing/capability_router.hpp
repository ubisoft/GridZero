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

    namespace internal {
        template<class TDomain, class TContract, class TSpace, std::size_t CellIndex, std::size_t... DimIs>
        void ValidateCell(::crg::models::ModelToken<TDomain> token, std::index_sequence<DimIs...>);

        template<class TDomain, class TContract, std::size_t... CellIs>
        void ValidateContractForHandle(::crg::models::ModelToken<TDomain> token, std::index_sequence<CellIs...>);

        template<class TDomain, class TContract>
        void ValidateContract();
    }

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
                RefreshCache();
            }

            void RefreshCache() {
                PopulatorList::Visit([](::crg::discovery::IArenaPopulator& populator) {
                    populator.Clear();
                });
                PopulatorList::Visit([](::crg::discovery::IArenaPopulator& populator) {
                    populator.Populate();
                });
            }
        };

        static ArenaBuilder& GetArenaBuilder() {
            static ArenaBuilder s_Builder{};
            return s_Builder;
        }

    public:
        static void RefreshCache() {
            GetArenaBuilder().RefreshCache();
        }

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

        template<class... TContracts>
        static void ValidateAll() {
            CRG_ASSERT_ENABLED_ONLY((internal::ValidateContract<TDomain, TContracts>(), ...));
        }
    };

    namespace internal {
        template<class TDomain, class TContract, class TSpace, std::size_t CellIndex, std::size_t... DimIs>
        void ValidateCell(::crg::models::ModelToken<TDomain> token, std::index_sequence<DimIs...>) {
            auto cap = CapabilityRouter<TDomain>::template Find<TContract>(
                token, TSpace::template GetCoordAtIndex<DimIs>(CellIndex)...);
            CRG_ASSERT(static_cast<bool>(cap),
                "CRG ValidateAll: a model has no binding for a contract at this cell.");
        }

        template<class TDomain, class TContract, std::size_t... CellIs>
        void ValidateContractForHandle(::crg::models::ModelToken<TDomain> token, std::index_sequence<CellIs...>) {
            using TSpace = typename CapabilityRoutingTraits<TDomain, TContract>::SpaceType;
            using DimSeq = std::make_index_sequence<TSpace::Dimensions>;
            (ValidateCell<TDomain, TContract, TSpace, CellIs>(token, DimSeq{}), ...);
        }

        template<class TDomain, class TContract>
        void ValidateContract() {
            using TSpace     = typename CapabilityRoutingTraits<TDomain, TContract>::SpaceType;
            using CellSeq    = std::make_index_sequence<TSpace::Volume>;
            using DenseStore = ::crg::core::DenseIndexStore<TDomain>;

            const ::crg::u32 modelCount = DenseStore::Size();
            for (::crg::u32 slot = 0; slot < modelCount; ++slot) {
                ::crg::models::ModelToken<TDomain> token{ ::crg::models::DenseModelID<TDomain>{ slot } };
                ValidateContractForHandle<TDomain, TContract>(token, CellSeq{});
            }
        }
    } // namespace internal

} // namespace crg::routing
