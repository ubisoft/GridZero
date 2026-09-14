// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// capability_space.hpp — N-dimensional enum-keyed capability routing space

#pragma once
#include "crg/core/at.hpp"
#include "crg/core/enum_traits.hpp"
#include <cstddef>
#include <tuple>
#include <utility>
#include <type_traits>

namespace crg::routing {

    template<class... TAxes>
    struct CapabilitySpace {
        using AxisTuple = std::tuple<TAxes...>;

        // Each axis must be an enum type with a valid EnumTraits<T>::Count > 0.
        static_assert((std::is_enum_v<TAxes> && ...),
            "CRG: All CapabilitySpace axes must be enum types.");
        static_assert(((::crg::EnumTraits<TAxes>::Count > 0) && ...),
            "CRG: Each routing axis must have at least one value (EnumTraits::Count > 0).");
        // Verify dense mapping if Values[] is provided in the specialization.
        static_assert((::crg::AxisDensityCheck<TAxes> && ...),
            "CRG: Routing axis does not map to a contiguous 0..N-1 range. "
            "Ensure EnumTraits::Values[] lists enumerators in dense order "
            "and EnumTraits::ToDense maps each to its index.");

        static constexpr std::size_t Dimensions = sizeof...(TAxes);
        static constexpr std::size_t Volume     = (Dimensions == 0) ? 1 : (::crg::EnumTraits<TAxes>::Count * ... * 1);

        template<std::size_t TDimIdx>
        static constexpr std::size_t GetStride() {
            if constexpr (Dimensions == 0 || TDimIdx >= Dimensions - 1) {
                return 1;
            } else {
                constexpr std::size_t dims[] = { ::crg::EnumTraits<TAxes>::Count... };
                std::size_t stride = 1;
                for (std::size_t i = TDimIdx + 1; i < Dimensions; ++i) {
                    stride *= dims[i];
                }
                return stride;
            }
        }

        template<std::size_t TDimIdx>
        static constexpr auto GetCoordAtIndex(std::size_t index) {
            if constexpr (Dimensions == 0) return 0;
            else {
                using AxisType = std::tuple_element_t<TDimIdx, AxisTuple>;
                return static_cast<AxisType>((index / GetStride<TDimIdx>()) % ::crg::EnumTraits<AxisType>::Count);
            }
        }

        // Sole public entry point: every routed offset includes the dense model slot
        // as the outermost axis. There is no "pure geometry" overload — nothing in
        // CRG needs an offset without a model, so none is exposed.
        template<typename... TCoords>
        static constexpr std::size_t ComputeOffset(std::size_t modelIndex, TCoords... coords) {
            return modelIndex * Volume + ComputeOffsetWithinModel(coords...);
        }

    private:
        static constexpr std::size_t ComputeOffsetWithinModel() { return 0; }

        template<typename... TCoords>
        static constexpr std::size_t Horner(TCoords... coords) {
            if constexpr (Dimensions == 0) return 0;
            else {
                // Route each coordinate through ToDense so enums with custom numeric
                // values still produce a valid 0..Count-1 index.  The call is constexpr
                // and inlined — no branch, no overhead.
                std::size_t c[] = { ::crg::internal::AxisToDense(coords)... };
                constexpr std::size_t dims[] = { ::crg::EnumTraits<TAxes>::Count... };

                std::size_t offset = 0;
                for (std::size_t i = 0; i < Dimensions; ++i) {
                    offset = offset * dims[i] + c[i];
                }
                return offset;
            }
        }

        template<typename... TCoords>
        static constexpr std::size_t ComputeOffsetWithinModel(TCoords... coords) {
            if constexpr (Dimensions == 0) return 0;
            else {
                static_assert(sizeof...(TCoords) == Dimensions, "CRG Error: Coordinate count mismatch.");
                return Horner(coords...);
            }
        }

        template<std::size_t TIndex, class TIdxSeq>
        struct MakeAtInternal;

        template<std::size_t TIndex, std::size_t... TDimIs>
        struct MakeAtInternal<TIndex, std::index_sequence<TDimIs...>> {
            using Type = ::crg::At<GetCoordAtIndex<TDimIs>(TIndex)...>;
        };

    public:
        template<std::size_t TIndex>
        using AtType = std::conditional_t<
            (Dimensions == 0),
            ::crg::At<>,
            typename MakeAtInternal<TIndex, std::make_index_sequence<Dimensions>>::Type
        >;
    };

} // namespace crg::routing
