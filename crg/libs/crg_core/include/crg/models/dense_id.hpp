// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// dense_id.hpp — typed u32 slot token for a CRG domain

#pragma once
#include "crg/core/types.hpp"
#include <limits>

namespace crg::models {

    template<typename TDomain>
    struct ModelToken;

    // Placed here (not dense_index.hpp) to break an include cycle:
    // dense_index.hpp needs ModelNodeBase, which does not need DenseID.
    inline constexpr ::crg::u32 InvalidModelSlot = std::numeric_limits<::crg::u32>::max();

    namespace internal {

        template<typename TDomain>
        struct DenseID {
            DenseID() = default;
            explicit DenseID(u32 value) : m_Value(value) {}

            inline bool IsValid() const {
                return m_Value != InvalidModelSlot;
            }

            inline u32 GetValue() const {
                return m_Value;
            }

            inline bool operator==(const DenseID& other) const {
                return m_Value == other.m_Value;
            }

            inline bool operator!=(const DenseID& other) const {
                return !(*this == other);
            }

        private:
            friend struct ::crg::models::ModelToken<TDomain>;

            u32 m_Value{ InvalidModelSlot };
        };

    }

    template<typename TDomain>
    using DenseModelID = internal::DenseID<TDomain>;

}
