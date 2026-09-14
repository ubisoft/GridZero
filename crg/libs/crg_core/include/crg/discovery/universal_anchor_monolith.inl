// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

#pragma once
#include "crg/core/config.hpp"
#include "crg/core/hash.hpp"

namespace crg::discovery {

    /**
     * @brief Monolithic Implementation.
     * Provides O(1) access to a static-local registry head.
     */
    template<class T> 
    struct UniversalAnchor {
        /**
         * @brief Returns the unique instance for the current process.
         */
        static T& Get() {

#if CRG_MISSING_TYPEHASH_AS_ERROR
            static_assert(sizeof(::crg::hash::TypeHash<T>) > 0, 
                "CRG Error: Type not registered. Ensure CRG_DECLARE_DOMAIN, CRG_DECLARE_MODEL, or CRG_DECLARE_CONTRACT is used.");
#endif

            static T m_Instance{};
            return m_Instance;
        }
    };

} // namespace crg::discovery
