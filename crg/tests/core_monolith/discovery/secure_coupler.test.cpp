// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// =============================================================================
// UNIT TEST: SECURE PLUGIN COUPLER (Floyd's cycle detection)
// =============================================================================

#include "catch.hpp"
#include "crg/discovery/secure_plugin_coupler.hpp"

using crg::discovery::SecurePluginCoupler;

namespace {
    struct TestNode {
        TestNode* m_Next{nullptr};
    };

    using Coupler = SecurePluginCoupler<TestNode>;
}

TEST_CASE("SecurePluginCoupler: nullptr head is safe with 0 nodes", "[discovery][secure_coupler]") {
    auto result = Coupler::Inspect(nullptr);
    REQUIRE_FALSE(result.hasCycle);
    REQUIRE(result.nodeCount == 0);
    REQUIRE(Coupler::IsSafe(nullptr));
}

TEST_CASE("SecurePluginCoupler: single node with null next is safe", "[discovery][secure_coupler]") {
    TestNode a;
    auto result = Coupler::Inspect(&a);
    REQUIRE_FALSE(result.hasCycle);
    REQUIRE(result.nodeCount == 1);
    REQUIRE(Coupler::IsSafe(&a));
}

TEST_CASE("SecurePluginCoupler: linear chain of 3 nodes is safe", "[discovery][secure_coupler]") {
    TestNode a, b, c;
    a.m_Next = &b;
    b.m_Next = &c;

    auto result = Coupler::Inspect(&a);
    REQUIRE_FALSE(result.hasCycle);
    REQUIRE(result.nodeCount == 3);
    REQUIRE(Coupler::IsSafe(&a));
}

TEST_CASE("SecurePluginCoupler: self-loop detected as cycle", "[discovery][secure_coupler]") {
    TestNode a;
    a.m_Next = &a;

    auto result = Coupler::Inspect(&a);
    REQUIRE(result.hasCycle);
    REQUIRE_FALSE(Coupler::IsSafe(&a));
}

TEST_CASE("SecurePluginCoupler: 2-node cycle detected", "[discovery][secure_coupler]") {
    TestNode a, b;
    a.m_Next = &b;
    b.m_Next = &a;

    auto result = Coupler::Inspect(&a);
    REQUIRE(result.hasCycle);
    REQUIRE_FALSE(Coupler::IsSafe(&a));
}

TEST_CASE("SecurePluginCoupler: cycle in the middle of chain detected", "[discovery][secure_coupler]") {
    TestNode a, b, c;
    a.m_Next = &b;
    b.m_Next = &c;
    c.m_Next = &b;  // b→c→b cycle

    auto result = Coupler::Inspect(&a);
    REQUIRE(result.hasCycle);
    REQUIRE_FALSE(Coupler::IsSafe(&a));
}

TEST_CASE("SecurePluginCoupler: IsSafe returns true for acyclic, false for cyclic", "[discovery][secure_coupler]") {
    TestNode a, b, c;
    a.m_Next = &b;
    b.m_Next = &c;

    REQUIRE(Coupler::IsSafe(nullptr));
    REQUIRE(Coupler::IsSafe(&a));

    c.m_Next = &a;  // introduce cycle
    REQUIRE_FALSE(Coupler::IsSafe(&a));
}
