// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// enum_traits.hpp — Compile-time metadata trait template for enum types
//
// Axis contract for CapabilitySpace:
//   • Count   — number of distinct axis values. Required.
//   • ToDense — maps an enum value to a contiguous 0..Count-1 index. Default = identity
//               (static_cast<std::size_t>). Override when your enum has custom numeric values.
//   • Values  — optional constexpr array listing every enumerator in dense order (v0, v1, …).
//               When present, CapabilitySpace validates at compile-time that
//               ToDense(Values[i]) == i for all i, enforcing the dense-from-0 contract.
//               Without Values, the contract is assumed (backward-compatible with { Count = N }).

#pragma once
#include <cstddef>
#include <type_traits>

namespace crg {

    // Primary template: users specialize this for each axis enum.
    template<typename T>
    struct EnumTraits;

    // ---------------------------------------------------------------------------
    // Compile-time axis validation helper
    // Checks that EnumTraits<T>::Values[] (when present) maps bijectively onto
    // 0..Count-1 via EnumTraits<T>::ToDense.  Fires a static_assert on violation.
    // ---------------------------------------------------------------------------

    namespace internal {

        // Trait: does EnumTraits<T> expose a Values array?
        template<typename T, typename = void>
        struct HasAxisValues : std::false_type {};

        template<typename T>
        struct HasAxisValues<T,
            std::void_t<decltype(EnumTraits<T>::Values)>>
            : std::true_type {};

        // Trait: does EnumTraits<T> expose a custom ToDense?
        template<typename T, typename = void>
        struct HasToDense : std::false_type {};

        template<typename T>
        struct HasToDense<T,
            std::void_t<decltype(EnumTraits<T>::ToDense(std::declval<T>()))>>
            : std::true_type {};

        // Default identity mapper (used when no ToDense is provided).
        template<typename T>
        constexpr std::size_t AxisToDense(T v) {
            if constexpr (HasToDense<T>::value) {
                return EnumTraits<T>::ToDense(v);
            } else {
                return static_cast<std::size_t>(v);
            }
        }

        // Validates at compile-time that AxisToDense(Values[i]) == i for all i.
        // Returns true if validation passes or if Values is absent (contract assumed).
        template<typename T, std::size_t... Is>
        constexpr bool ValidateDenseMapping(std::index_sequence<Is...>) {
            return ((AxisToDense(EnumTraits<T>::Values[Is]) == Is) && ...);
        }

        template<typename T>
        constexpr bool ValidateAxisDensity() {
            if constexpr (HasAxisValues<T>::value) {
                constexpr std::size_t N = EnumTraits<T>::Count;
                return ValidateDenseMapping<T>(std::make_index_sequence<N>{});
            } else {
                return true; // assumed dense — backward-compatible
            }
        }

    } // namespace internal

    // Public check: trigger a static_assert in CapabilitySpace or at specialization site.
    template<typename T>
    inline constexpr bool AxisDensityCheck = internal::ValidateAxisDensity<T>();

} // namespace crg
