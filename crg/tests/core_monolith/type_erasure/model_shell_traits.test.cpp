// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// =============================================================================
// CRG UNIT TEST: MODEL SHELL TRAITS
// =============================================================================

#include "catch.hpp"
#include "crg/crg.hpp"

using namespace crg;

// =============================================================================
// 1. FIXTURES
// =============================================================================

namespace crg::shell::test_traits {

    // No ModelShellMutabilityTraits specialization: exercises the default.
    struct ReadOnlyPartition {};

    struct PingModel {};

    using PingShell = ModelShell<ReadOnlyPartition>;

    struct IPinger {
        virtual int Ping(const PingShell& shell) const = 0;
        virtual void Notify(const PingShell& shell) const = 0;
        virtual ~IPinger() = default;
    };

}

CRG_DECLARE_DOMAIN(crg::shell::test_traits::ReadOnlyPartition)
CRG_DECLARE_DOMAIN_MODELS(crg::shell::test_traits::ReadOnlyPartition,
    crg::shell::test_traits::PingModel,
    crg::shell::test_traits::IPinger)

// =============================================================================
// 2. STATIC (COMPILE-TIME) CHECKS
// =============================================================================

namespace crg::shell::test_traits {

    // --- ModelShellTraits defaults forward the config macros verbatim ---
    static_assert(ModelShellTraits<ReadOnlyPartition>::MaxSize == CRG_MODEL_SHELL_SIZE,
        "ModelShellTraits default MaxSize must equal CRG_MODEL_SHELL_SIZE");
    static_assert(ModelShellTraits<ReadOnlyPartition>::MaxAlignment == CRG_CACHE_LINE_SIZE,
        "ModelShellTraits default MaxAlignment must equal CRG_CACHE_LINE_SIZE");

    // --- ModelShellMutabilityTraits defaults to immutable ---
    static_assert(ModelShellMutabilityTraits<ReadOnlyPartition>::IsMutable == false,
        "ModelShellMutabilityTraits must default to IsMutable = false");

    // --- IsOptional ---
    static_assert(crg::IsOptional<int> == false, "int is not std::optional");
    static_assert(crg::IsOptional<std::optional<int>> == true, "std::optional<int> is std::optional");

    // --- TryInvokeResult_t: void passes through unchanged ---
    static_assert(std::is_void_v<crg::TryInvokeResult_t<void>>,
        "TryInvokeResult_t<void> must be void");

    // --- TryInvokeResult_t: a plain type gets wrapped in std::optional ---
    static_assert(std::is_same_v<crg::TryInvokeResult_t<int>, std::optional<int>>,
        "TryInvokeResult_t<int> must be std::optional<int>");

    // --- TryInvokeResult_t: an already-optional type is NOT double-wrapped ---
    static_assert(std::is_same_v<crg::TryInvokeResult_t<std::optional<float>>, std::optional<float>>,
        "TryInvokeResult_t<std::optional<float>> must stay std::optional<float>, "
        "never std::optional<std::optional<float>>");

    // --- ModelShellMethodTraits: extracts Interface/ReturnType from a const routed method ---
    using PingTraits = ModelShellMethodTraits<ReadOnlyPartition, decltype(&IPinger::Ping)>;
    static_assert(std::is_same_v<PingTraits::Interface, IPinger>,
        "ModelShellMethodTraits must extract the owning interface");
    static_assert(std::is_same_v<PingTraits::ReturnType, int>,
        "ModelShellMethodTraits must extract the method's return type");

    using NotifyTraits = ModelShellMethodTraits<ReadOnlyPartition, decltype(&IPinger::Notify)>;
    static_assert(std::is_same_v<NotifyTraits::ReturnType, void>,
        "ModelShellMethodTraits must extract void return types too");

}

// =============================================================================
// 3. RUNTIME CHECKS (same facts, surfaced through Catch so a regression shows
// up in the test report rather than only as a build break)
// =============================================================================

TEST_CASE("ModelShellTraits: buffer sizing defaults match the config macros", "[type_erasure][traits]") {
    using Traits = crg::shell::ModelShellTraits<crg::shell::test_traits::ReadOnlyPartition>;

    REQUIRE(Traits::MaxSize == CRG_MODEL_SHELL_SIZE);
    REQUIRE(Traits::MaxAlignment == CRG_CACHE_LINE_SIZE);
}

TEST_CASE("ModelShellMutabilityTraits: defaults to immutable", "[type_erasure][traits]") {
    REQUIRE(crg::shell::ModelShellMutabilityTraits<crg::shell::test_traits::ReadOnlyPartition>::IsMutable == false);
}

TEST_CASE("IsOptional: detects std::optional, rejects everything else", "[type_erasure][traits]") {
    REQUIRE(crg::IsOptional<std::optional<int>> == true);
    REQUIRE(crg::IsOptional<int> == false);
    REQUIRE(crg::IsOptional<crg::shell::test_traits::PingModel> == false);
}
