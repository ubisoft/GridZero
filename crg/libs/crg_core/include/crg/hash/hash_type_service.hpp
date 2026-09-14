// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// hash_type_service.hpp — Brain capability interface for cross-ABI type-name hashing

#pragma once
#include "crg/capabilities/capability.hpp"
#include "crg/core/types.hpp"
#include "crg/core/hash.hpp"

namespace crg::hash {

    struct IHashTypeService {
        virtual crg::u64 HashType(const char* typeName) const noexcept = 0;
        virtual ~IHashTypeService() = default;
    };

} // namespace crg::hash

CRG_DECLARE_CONTRACT(crg::hash::IHashTypeService)
