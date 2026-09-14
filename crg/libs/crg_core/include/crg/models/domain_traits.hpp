// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// domain_traits.hpp — compile-time identity and model-key contract for domains

#pragma once
#include "crg/core/config.hpp"
#include "crg/models/domain_key.hpp"
#include "crg/models/model_key.hpp"
#include <cstddef>

namespace crg::models {

    template <typename TDomain>
    struct DomainTraits {

        using DomainKeyType = ::crg::u64;
        using ModelKeyType  = ::crg::u64;

    private:
        // Deferred: only instantiated when Identity is odr-used, so a domain tag
        // used solely as a ModelShell parameter does not require CRG_DECLARE_DOMAIN().
        static constexpr DomainKeyType ComputeIdentity() noexcept {
            static_assert(sizeof(DomainKey<TDomain>) > 0,
                "\n[CRG ERROR] Domain not registered. Use CRG_DECLARE_DOMAIN() in a header.\n");
            return DomainKey<TDomain>::Value;
        }

    public:
        static constexpr DomainKeyType Identity = ComputeIdentity();

        template <typename TModel>
        static constexpr ModelKeyType ResolveModelKey() {
            static_assert(sizeof(ModelKey<TModel>) > 0,
                "\n[CRG ERROR] Model not registered. List the model in CRG_DECLARE_DOMAIN_MODELS(TDomain, M1, M2, ...).\n");
            return ModelKey<TModel>::Value;
        }

        template <typename TModel>
        static constexpr ModelKeyType TypeKey = ResolveModelKey<TModel>();
    };

} // namespace crg::models
