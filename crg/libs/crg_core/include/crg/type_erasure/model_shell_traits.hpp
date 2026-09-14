// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// model_shell_traits.hpp — fixed-size buffer capacity and method-signature traits for ModelShell

#pragma once
#include "crg/core/config.hpp"
#include <type_traits>
#include <optional>

// Forward declaration of ModelShell to resolve circular dependencies
namespace crg::shell {
    template<typename TDomain>
    class ModelShell;
}

namespace crg::shell {

    template<typename TDomain>
    struct ModelShellTraits {
        static constexpr std::size_t MaxSize             = CRG_MODEL_SHELL_SIZE;
        static constexpr std::size_t MaxAlignment        = CRG_CACHE_LINE_SIZE;
    };

    template<typename TDomain>
    struct ModelShellMutabilityTraits {
        static constexpr bool IsMutable = false;
    };

}

namespace crg {

    template<typename T>
    using TryInvokeResult_t = std::conditional_t<std::is_void_v<T>, void, std::optional<T>>;

    template <typename...>
    constexpr bool always_false_v = false;

    template<typename TDomain, typename TFunc>
    struct ModelShellMethodTraits {
        static_assert(always_false_v<TFunc>,
            "CRG ARCHITECTURE VIOLATION: Routed interface methods MUST be marked as 'const'. Stateless behavior is strictly enforced.");
    };

    template<typename TDomain, typename R, class I, typename... Args>
    struct ModelShellMethodTraits<TDomain, R (I::*)(const shell::ModelShell<TDomain>&, Args...) const> {
        using Interface  = I;
        using ReturnType = R;
    };

}
