// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// hash.hpp — compile-time FNV-1a hashing and type hash registry

#pragma once
#include "crg/core/config.hpp"
#include "crg/core/types.hpp"
#include <string_view>

namespace crg::hash::internal {

    constexpr u64 Fnv1aBaseline = 14695981039346656037ull;
    constexpr u64 Fnv1aPrime    = 1099511628211ull;

    constexpr u64 HashString(const char* str) {
        u64 hash = Fnv1aBaseline;
        for (std::size_t i = 0; str[i] != '\0'; ++i) {
            hash ^= static_cast<u64>(str[i]);
            hash *= Fnv1aPrime;
        }
        return hash;
    }

    constexpr u64 HashString(std::string_view sv) {
        u64 hash = Fnv1aBaseline;
        for (char c : sv) {
            hash ^= static_cast<u64>(c);
            hash *= Fnv1aPrime;
        }
        return hash;
    }

    // Strips a leading "::" so ::foo::Bar and foo::Bar hash identically — the
    // common spelling variant when a type is named from global scope. Other
    // spelling variants (embedded whitespace, aliases, template-arg
    // formatting) are not canonicalized: hashing the explicit #Type stringize
    // is deliberate, since it is preprocessor-level and therefore identical
    // across MSVC/Clang/GCC — the property that makes the routing u64
    // exchangeable cross-compiler and cross-language. Compiler-intrinsic type
    // names (__FUNCSIG__/__PRETTY_FUNCTION__) are rejected on the same
    // grounds: their spelling is compiler-specific.
    constexpr std::string_view RemoveLeadingGlobalNamespace(std::string_view full_name) {
        return (full_name.size() >= 2 && full_name[0] == ':' && full_name[1] == ':')
            ? full_name.substr(2)
            : full_name;
    }

    // ASCII-only tolower: safe in constexpr, no locale dependency.
    constexpr char ToLowerASCII(char c) noexcept {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
    }

    // Case-insensitive variant — used for file extension routing (.JSON == .json).
    constexpr u64 HashStringLower(std::string_view sv) noexcept {
        u64 hash = Fnv1aBaseline;
        for (char c : sv) {
            hash ^= static_cast<u64>(ToLowerASCII(c));
            hash *= Fnv1aPrime;
        }
        return hash;
    }

}

namespace crg::hash {

    using internal::HashString;
    using internal::HashStringLower;

    template <typename T>
    struct TypeHashBase {};

    template <typename T>
    struct TypeHash : TypeHashBase<T> {};

}

#define CRG_INTERNAL_DECLARE_HASH_CUSTOM(Type, NameStr)                   \
    namespace crg::hash {                                                 \
        template <>                                                       \
        struct TypeHash<Type> {                                           \
            static constexpr ::crg::u64 Value =                             \
                internal::HashString(internal::RemoveLeadingGlobalNamespace(NameStr)); \
            CRG_HASH_NAME_ENABLED_ONLY(static constexpr const char* Name = NameStr;) \
        };                                                                \
    }

// Hashes the full #Type text, no truncation — used for domains, where identity
// must stay language-neutral and collision-free across the whole qualified name.
#define CRG_DECLARE_HASH(Type) CRG_INTERNAL_DECLARE_HASH_CUSTOM(Type, #Type)

#define CRG_DECLARE_CONTRACT(ContractType) \
    CRG_DECLARE_HASH(ContractType)
