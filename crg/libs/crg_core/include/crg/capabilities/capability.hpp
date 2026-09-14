// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// capability.hpp — Base template wrapping a contract into a Capability

#pragma once
#include <type_traits>

namespace crg::capabilities {

    template<class TContract, class TConfig = void>
    struct Capability : public TContract {
        using ContractType = TContract;
        using ConfigType    = TConfig;

        TConfig m_Config;
    };

    template<class TContract>
    struct Capability<TContract, void> : public TContract {
        using ContractType = TContract;
        using ConfigType    = void;
    };

    template<typename TType, typename = void>
    struct HasConfigType : std::false_type {};

    template<typename TType>
    struct HasConfigType<TType, std::void_t<typename TType::ConfigType>> {
        static constexpr bool Value = !std::is_same_v<typename TType::ConfigType, void>;
    };

} // namespace crg::capabilities
