// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// arena_populator.hpp — interface for injecting rules into TensorArena

#pragma once
#include "crg/discovery/node_link.hpp"

namespace crg::discovery {

    struct IArenaPopulator {
        virtual ~IArenaPopulator() = default;
        virtual void Populate() = 0;
    };

    template<class TDomain>
    struct DomainArenaPopulator : public NodeLink<DomainArenaPopulator<TDomain>, IArenaPopulator> {};

} // namespace crg::discovery
