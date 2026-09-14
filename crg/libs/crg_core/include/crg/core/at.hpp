// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// at.hpp — compile-time coordinate tag for N-D capability routing

#pragma once
#include <cstddef>
#include <utility>

namespace crg {

    template<auto... TValues>
    struct At {};

    template<class TSpace, std::size_t Index, class IdxSeq>
    struct MakeAtImpl;

    template<class TSpace, std::size_t Index, std::size_t... DimIs>
    struct MakeAtImpl<TSpace, Index, std::index_sequence<DimIs...>> {
        using Type = At<TSpace::template GetCoordAtIndex<DimIs>(Index)...>;
    };

    template<class TSpace, std::size_t Index>
    using MakeAt = typename MakeAtImpl<TSpace, Index, std::make_index_sequence<TSpace::Dimensions>>::Type;

} // namespace crg
