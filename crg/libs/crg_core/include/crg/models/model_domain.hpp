// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// model_domain.hpp — bridge from a Model type to its owning Domain

#pragma once

namespace crg::models {

    template<class TModel>
    struct ModelDomain; // intentionally incomplete: unregistered models produce a clear diagnostic

    template<class TModel>
    using ModelDomainT = typename ModelDomain<TModel>::Type;

} // namespace crg::models
