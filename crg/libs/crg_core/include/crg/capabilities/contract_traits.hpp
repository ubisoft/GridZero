// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// contract_traits.hpp — Contract detection traits and invocation mixins for DOD and OOP

#pragma once
#include "crg/core/config.hpp"
#include <type_traits>
#include <utility>

namespace crg::capabilities {

    template<typename TType, typename = void>
    struct IsStaticContractImpl : std::false_type {};

    template<typename TType>
    struct IsStaticContractImpl<TType, std::void_t<typename TType::Params>>
        : std::bool_constant<!std::is_polymorphic_v<TType>> {};

    template<typename TType>
    inline constexpr bool IsStaticContract = IsStaticContractImpl<TType>::value;

    template<typename TImpl, typename TParams, typename = std::void_t<>>
    struct HasStaticExecuteImpl : std::false_type {};

    template<typename TImpl, typename TParams>
    struct HasStaticExecuteImpl<TImpl, TParams, std::void_t<
        decltype(TImpl::Execute(std::declval<TParams&>()))
    >> : std::true_type {};

    template<typename TImpl, typename TParams>
    inline constexpr bool HasStaticExecute = HasStaticExecuteImpl<TImpl, TParams>::value;

    template<typename TDerived, typename TContract, bool IsStatic>
    struct InvocationMixin;

    template<typename TDerived, typename TContract>
    struct InvocationMixin<TDerived, TContract, true> {
    private:
        using Params = typename TContract::Params;
        using Action = void (*)(Params&);

        Action GetTarget() const {
            return static_cast<const TDerived&>(*this).GetTarget();
        }

    public:
        inline void operator()(Params& params) const {
            CRG_ASSERT(GetTarget() != nullptr, "operator() on unbound DOD handle — use TryInvoke for optional dispatch");
            GetTarget()(params);
        }

        inline void TryInvoke(Params& params) const {
            if (Action ptr = GetTarget()) ptr(params);
        }

        inline explicit operator bool() const {
            return GetTarget() != nullptr;
        }
    };

    template<typename TDerived, typename TContract>
    struct InvocationMixin<TDerived, TContract, false> {
    private:
        const TContract* GetTarget() const {
            return static_cast<const TDerived&>(*this).GetTarget();
        }

    public:
        template<class... TArgs, typename = std::enable_if_t<std::is_invocable_v<const TContract&, TArgs...>>>
        inline void operator()(TArgs&&... args) const {
            if (const TContract* ptr = GetTarget()) {
                (*ptr)(std::forward<TArgs>(args)...);
            }
        }

        template<auto FuncPtr, class... TArgs>
        inline void Invoke(TArgs&&... args) const {
            if (const TContract* ptr = GetTarget()) {
                (ptr->*FuncPtr)(std::forward<TArgs>(args)...);
            }
        }

        inline explicit operator bool() const {
            return GetTarget() != nullptr;
        }
    };

} // namespace crg::capabilities
