// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// binding_target.hpp — Capability invocation target, function-pointer or interface-pointer

#pragma once
#include "crg/capabilities/contract_traits.hpp"

namespace crg::capabilities {

    template<typename TContract, bool TIsStatic = IsStaticContract<TContract>>
    struct BindingTarget;

    template<typename TContract>
    struct BindingTarget<TContract, true>
        : public InvocationMixin<BindingTarget<TContract, true>, TContract, true> {

        using Params = typename TContract::Params;
        using Action = void (*)(Params&);

        Action m_Target{ nullptr };

        inline Action GetTarget() const { return m_Target; }

        template<typename TImpl, typename U>
        inline void SetTarget(const U*) {
            m_Target = &TImpl::Execute;
        }

        inline explicit operator bool() const { return m_Target != nullptr; }
    };

    template<typename TContract>
    struct BindingTarget<TContract, false>
        : public InvocationMixin<BindingTarget<TContract, false>, TContract, false> {

        const TContract* m_Target{ nullptr };

        inline const TContract* GetTarget() const { return m_Target; }

        template<typename TImpl>
        inline void SetTarget(const TContract* instance) {
            m_Target = instance;
        }

        inline explicit operator bool() const { return m_Target != nullptr; }
    };

}
