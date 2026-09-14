// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// capability_handle.hpp — typed handle binding a caller to a capability

#pragma once
#include "crg/capabilities/binding_target.hpp"

namespace crg::capabilities {

    template<class TContract, bool IsStatic = IsStaticContract<TContract>>
    struct CapabilityHandle;

    template<class TContract>
    struct CapabilityHandle<TContract, true>
        : public InvocationMixin<CapabilityHandle<TContract, true>, TContract, true> {
    private:
        using Mixin  = InvocationMixin<CapabilityHandle<TContract, true>, TContract, true>;
        using Target = BindingTarget<TContract, true>;
        using Params = typename TContract::Params;
        using Action = void (*)(Params&);

        friend Mixin;
        inline Action GetTarget() const { return m_Target; }
        Action m_Target{ nullptr };

    public:
        inline CapabilityHandle& operator=(const Target* target) {
            m_Target = target ? target->m_Target : nullptr;
            return *this;
        }
    };

    template<class TContract>
    struct CapabilityHandle<TContract, false>
        : public InvocationMixin<CapabilityHandle<TContract, false>, TContract, false> {
    private:
        using Mixin  = InvocationMixin<CapabilityHandle<TContract, false>, TContract, false>;
        using Target = BindingTarget<TContract, false>;

        friend Mixin;
        inline const TContract* GetTarget() const { return m_Ptr; }
        const TContract* m_Ptr{ nullptr };

    public:
        inline CapabilityHandle& operator=(const Target* target) {
            m_Ptr = target ? target->m_Target : nullptr;
            return *this;
        }

        inline CapabilityHandle& operator=(const TContract* ptr) {
            m_Ptr = ptr;
            return *this;
        }

        inline const TContract* operator->() const { return m_Ptr; }
        inline const TContract& operator*()  const { return *m_Ptr; }
    };

} // namespace crg::capabilities
