// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// capability_binding.hpp — Binds model capabilities into tensor arena cells

#pragma once
#include "crg/core/at.hpp"
#include "crg/core/type_list.hpp"
#include "crg/discovery/arena_populator.hpp"
#include "crg/routing/capability_routing_traits.hpp"
#include "crg/routing/tensor_arena.hpp"
#include "crg/models/dense_id.hpp"
#include "crg/models/domain_traits.hpp"
#include "crg/models/model_key.hpp"
#include "crg/capabilities/binding_target.hpp"
#include "crg/capabilities/capability.hpp"
#include "crg/core/dense_index.hpp"
#include "crg/core/config.hpp"
#include <utility>
#include <algorithm>
#include <type_traits>

namespace crg::capabilities {

    // Bootstrap contract-reader: tolerates either arity ONLY to read ContractType,
    // which is inherited from Capability<TContract> and therefore identical
    // regardless of which arity a capability author chose. Not a dispatch
    // discriminant -- see CapImplSel below for the actual (Dims-only) dispatch.
    template<class TModel, template<class...> class TCap, class = void>
    struct CapContract { using Type = typename TCap<TModel, void>::ContractType; };

    template<class TModel, template<class...> class TCap>
    struct CapContract<TModel, TCap, std::void_t<typename TCap<TModel>::ContractType>>
        { using Type = typename TCap<TModel>::ContractType; };

    template<class TModel, template<class...> class TCap>
    using CapContractType = typename CapContract<TModel, TCap>::Type;

    template<class TDomain, class TModel, template<class...> class TCap>
    using CapSpace = typename ::crg::routing::CapabilityRoutingTraits<
        TDomain, CapContractType<TModel, TCap>>::SpaceType;

    // Pure dimensionality model: Dims == 0 binds TCap<TModel>; Dims > 0 binds
    // TCap<TModel, MakeAt<Space, TIs>>. Arity is never a dispatch input -- a
    // capability whose arity contradicts its contract's Dimensions is a hard
    // compile error (the author must conform), never silently reinterpreted.
    template<bool TZeroDim, class TModel, template<class...> class TCap, class TSpace, std::size_t TI>
    struct CapImplSel { using Type = TCap<TModel, ::crg::MakeAt<TSpace, TI>>; };

    template<class TModel, template<class...> class TCap, class TSpace, std::size_t TI>
    struct CapImplSel<true, TModel, TCap, TSpace, TI> { using Type = TCap<TModel>; };

    template<class TDomain, class TModel, template<class...> class TCap, std::size_t TI>
    using CapImplAt = typename CapImplSel<
        (CapSpace<TDomain, TModel, TCap>::Dimensions == 0),
        TModel, TCap, CapSpace<TDomain, TModel, TCap>, TI>::Type;

    template<class TDomain, class TModel, template<class...> class TCap, class TIdxSeq>
    struct CapabilityNode;

    template<class TDomain, class TModel, template<class...> class TCap, std::size_t... TIs>
    struct CapabilityNode<TDomain, TModel, TCap, std::index_sequence<TIs...>>
        : public CapImplAt<TDomain, TModel, TCap, TIs>...
    {
        using Contract      = CapContractType<TModel, TCap>;
        using Traits        = ::crg::routing::CapabilityRoutingTraits<TDomain, Contract>;
        using Space         = typename Traits::SpaceType;
        using Context       = ::crg::routing::ContextTypeOf<TDomain, Contract>;
        using Arena         = ::crg::routing::TensorArena<TDomain, Contract>;

        void FillArena(::crg::models::DenseModelID<TDomain> denseId) const {
            auto& arena = Arena::Get();
            const std::size_t baseIdx = static_cast<std::size_t>(denseId.GetValue()) * Space::Volume;

            if (arena.size() < baseIdx + Space::Volume) {
                arena.resize(baseIdx + Space::Volume);
            }

            ([&arena, baseIdx, this]() {
                using Impl = CapImplAt<TDomain, TModel, TCap, TIs>;
                auto& cell = arena[baseIdx + TIs];

                if constexpr (!std::is_polymorphic_v<Contract>) {
                    static_assert(::crg::capabilities::IsStaticContract<Contract>,
                        "CRG Architecture Error: Non-polymorphic contracts MUST declare a nested 'Params' structure.");

                    static_assert(::crg::capabilities::HasStaticExecute<Impl, typename Contract::Params>,
                        "CRG Architecture Error: DOD Capability implementations MUST provide a 'static Execute(Params&)' function.");
                }

                const Impl* implPtr = static_cast<const Impl*>(this);

                BindingTarget<Contract> target;
                target.template SetTarget<Impl>(implPtr);

                cell.template Bind<Impl>(target, implPtr);
            }(), ...);
        }

        void ClearArena() const {
            Arena::Get().clear();
        }
    };

    template<class TDomain, class TModel, template<class...> class... TCapabilities>
    struct CapabilityBinding : public ::crg::discovery::DomainArenaPopulator<TDomain> {
    private:
        using DomainTraits = ::crg::models::DomainTraits<TDomain>;
        using DenseIndex   = ::crg::core::DenseIndexStore<TDomain>;
        using DenseID      = ::crg::models::DenseModelID<TDomain>;

        static_assert(sizeof(::crg::models::ModelKey<TModel>) > 0,
            "\n[CRG ERROR] CapabilityBinding<TDomain, TModel, ...>: "
            "TModel was not declared via CRG_DECLARE_DOMAIN_MODELS(TDomain, TModel, ...).\n");

        template<template<class...> class TCap>
        static constexpr std::size_t VolOf = CapSpace<TDomain, TModel, TCap>::Volume;

        struct Unit : public CapabilityNode<TDomain, TModel, TCapabilities, std::make_index_sequence<VolOf<TCapabilities>>>... {
            void Fill(DenseID denseId) const {
                (CapabilityNode<TDomain, TModel, TCapabilities, std::make_index_sequence<VolOf<TCapabilities>>>::FillArena(denseId), ...);
            }
            void Clear() const {
                (CapabilityNode<TDomain, TModel, TCapabilities, std::make_index_sequence<VolOf<TCapabilities>>>::ClearArena(), ...);
            }
        } m_Unit{};

        // Must carry the same full-name pointer as CRG_DDM_BIND's ModelNode (model_key.hpp)
        // for the same (TDomain, TModel) pair — otherwise this node's GetName() reads empty
        // and DenseIndexStore::Refresh (core/dense_index.hpp) sees a name mismatch against
        // the domain-declared node and misreports a harmless re-registration as a collision.
        ::crg::models::internal::ModelNode<TDomain, ::crg::hash::TypeHash<TModel>::Value> m_ModelNode{
            CRG_HASH_NAME_ENABLED_ONLY(&::crg::hash::TypeHash<TModel>::Name)
        };

    public:
        bool CanPopulate() const noexcept override {
            constexpr auto Hash = DomainTraits::template TypeKey<TModel>;
            return DenseIndex::Find(Hash) != ::crg::core::InvalidDenseSlot;
        }

        void Populate() override {
            constexpr auto Hash = DomainTraits::template TypeKey<TModel>;

            DenseIndex::Refresh();

            const ::crg::u32 slot = DenseIndex::Find(Hash);
            if (slot == ::crg::core::InvalidDenseSlot) {
                return;
            }

            DenseID denseId{ slot };
            m_Unit.Fill(denseId);
        }

        void Clear() override {
            m_Unit.Clear();
        }
    };

} // namespace crg::capabilities
