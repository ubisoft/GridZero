// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// ═════════════════════════════════════════════════════════════════════════════
// STAGE 00 — Linker-Driven Discovery (NodeLink)
// ─────────────────────────────────────────────────────────────────────────────
// PROBLEM:
//   You have several implementations of an interface spread across .cpp files.
//   You want a caller to visit all of them without knowing how many exist or
//   where they live — no central registry, no Init() call, no header coupling.
//
// SOLUTION:
//   NodeLink<TNode, TContract>. Each implementation self-registers at link time
//   via a static instance. The linker wires a singly-linked chain at startup;
//   the caller just calls Visit(). No central file is ever touched.
//
// WHEN TO USE (without any other CRG layer):
//   - Service locator without a singleton
//   - Collecting all registered subsystems at startup
//   - Plugin systems where the caller must not know the implementors
//
// WHAT'S NEW:
//   Everything. This is Layer 2 (Discovery) in isolation.
//   No domain, no model, no capability, no routing tensor.
// ═════════════════════════════════════════════════════════════════════════════

#include "catch.hpp"
#include "crg/discovery/node_link.hpp"

// ─── 1. The interface — a plain virtual base ──────────────────────────────────
struct IGreeter {
    virtual const char* Greet() const = 0;
};

// ─── 2. The list type — the linker-driven registry ───────────────────────────
//   NodeLink<TNode, TContract>:
//     TNode     = the concrete list entry type (also the self-registering type)
//     TContract = the virtual interface exposed to visitors
//   Each static IGreeterList instance is linked into a chain at construction.
struct IGreeterList : crg::discovery::NodeLink<IGreeterList, IGreeter> {};

// ─── 3. Implementations anywhere in any .cpp ─────────────────────────────────
//   The anonymous namespace gives internal linkage.
//   The static instance triggers NodeLink's constructor → self-registration.
//   No factory, no extern, no coupling to the visitor.
namespace {
    struct HelloGreeter : IGreeterList {
        const char* Greet() const override { return "Hello"; }
    };
    struct HiGreeter : IGreeterList {
        const char* Greet() const override { return "Hi"; }
    };
    static const HelloGreeter s_Hello;
    static const HiGreeter    s_Hi;
}

// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Stage 00 — NodeLink visits all self-registered nodes", "[stage00][discovery]") {
    // Visit() iterates the linearized cache built from the static chain.
    // The caller doesn't know how many nodes exist.
    int count = 0;
    IGreeterList::Visit([&count](IGreeterList& node) {
        (void)node.Greet();
        ++count;
    });
    REQUIRE(count == 2);
}

TEST_CASE("Stage 00 — Visit is cancellable: returning false stops early", "[stage00][discovery]") {
    // If the callable returns bool, Visit checks it and stops on false.
    // Useful for "find first matching" patterns.
    int count = 0;
    IGreeterList::Visit([&count](IGreeterList&) -> bool {
        ++count;
        return false; // stop after the first node
    });
    REQUIRE(count == 1);
}

TEST_CASE("Stage 00 — RefreshCache is the DLL hot-reload hook", "[stage00][discovery]") {
    // In monolithic mode, RefreshCache re-walks the static chain — same result.
    // Its purpose is to re-linearize after a DLL is loaded or unloaded.
    IGreeterList::RefreshCache();
    int count = 0;
    IGreeterList::Visit([&count](IGreeterList&) { ++count; });
    REQUIRE(count == 2);
}
