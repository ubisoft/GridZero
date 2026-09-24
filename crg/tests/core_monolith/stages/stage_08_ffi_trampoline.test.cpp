// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// ═════════════════════════════════════════════════════════════════════════════
// STAGE 08 — FFI Trampoline: the C-ABI Proof
// ─────────────────────────────────────────────────────────────────────────────
// CLAIM TO VERIFY (CppCon Act I slide 1.8):
//   "ABI-neutrality emerges from decoupling — Rust/WASM could self-register."
//
// THIS IS FALSE for the NodeLink (Act I) layer.
//   NodeLink self-registration requires C++ CRTP + static_cast<TNode*>(this).
//   A Rust or WASM module cannot call a C++ constructor.
//
// WHERE IT IS TRUE: the Muscle DOD path (Act IV, Stage 07).
//   A DOD contract is: struct IFoo { struct Params { ... }; };
//   Its capability reduces to: void (*)(IFoo::Params&) — a plain C function pointer.
//   Any language that can export a C function can inject into this slot.
//
// THIS FILE PROVES:
//   1. IsStaticContract<T>   == true  when T has Params and no virtual (the C-ABI boundary)
//   2. IsStaticContract<T>   == false when T is polymorphic (vtable = C++ ABI, not C ABI)
//   3. A plain C function pointer satisfies a Muscle CapabilityHandle — the slot that
//      a Rust/WASM trampoline would fill at the binary boundary.
//   4. The NodeLink (Brain / virtual) path requires C++ ABI — Rust/WASM cannot
//      self-register without a C++ wrapper (the "Proxy Node" pattern).
//
// NARRATIVE CONSEQUENCE:
//   - Act I reward = C++ binary stability (MSVC ↔ Clang), not multi-language.
//   - Act IV reward = pure C-ABI, true multi-language by structural consequence.
//
// WHAT'S NEW vs Stage 07:
//   - Same Muscle contract shape as Stage 07 (no virtual, Params, single Execute).
//   - A plain C function pointer (standing in for a Rust/WASM export) fills the
//     CapabilityHandle slot directly, bypassing the router.
//   - NodeLink's C++-only registration path is checked as the boundary that does
//     NOT extend to foreign code — the contrast that makes the Muscle result matter.
// ═════════════════════════════════════════════════════════════════════════════

#include "catch.hpp"
#include "crg/capabilities/contract_traits.hpp"
#include "crg/capabilities/capability_handle.hpp"
#include "crg/capabilities/binding_target.hpp"
#include "crg/discovery/node_link.hpp"

// ─── Contracts ───────────────────────────────────────────────────────────────

// Muscle (DOD): no virtual → IsStaticContract == true → reduces to void(*)(Params&)
struct IMove {
    struct Params { float m_Speed{ 0.f }; };
};

// Brain (OOP): virtual → IsStaticContract == false → requires C++ vtable
struct ILog {
    struct Params { const char* m_Msg{ nullptr }; };
    virtual void Execute(Params&) const = 0;
    virtual ~ILog() = default;
};

// ─── 1. Contract classification at compile time ───────────────────────────────

TEST_CASE("Stage 08 — IsStaticContract is true for plain struct with Params (no virtual)", "[stage08][ffi]") {
    // A Muscle contract has no virtual methods → its capability is a raw fn ptr.
    // This is the structural invariant that makes C-ABI possible at Act IV.
    static_assert(crg::capabilities::IsStaticContract<IMove>,
        "IMove must be a DOD contract: struct with Params, no virtual");
    SUCCEED("IMove is a DOD contract");
}

TEST_CASE("Stage 08 — IsStaticContract is false for polymorphic contract (vtable present)", "[stage08][ffi]") {
    // A Brain contract has virtual methods → requires C++ ABI (vtable layout).
    // Rust/WASM cannot fill this slot without a C++ wrapper.
    static_assert(!crg::capabilities::IsStaticContract<ILog>,
        "ILog must NOT be a DOD contract: it has virtual methods");
    SUCCEED("ILog is NOT a DOD contract — it carries a C++ vtable");
}

// ─── 2. A plain C function pointer satisfies the Muscle slot ─────────────────
// This is the exact shape a Rust `extern "C" fn` or WASM export would have.

namespace {
    // Simulates what a Rust `extern "C"` export or WASM trampoline looks like:
    // a plain C function, no name mangling, no vtable, no C++ runtime dependency.
    void ForeignExecute(IMove::Params& p) {
        p.m_Speed = 42.f;
    }
}

TEST_CASE("Stage 08 — raw C function pointer satisfies a Muscle CapabilityHandle", "[stage08][ffi]") {
    // BindingTarget<IMove, true> wraps the fn ptr — this is what CapabilityRouter
    // stores in the routing tensor. We bypass the router here to test the slot directly.
    crg::capabilities::BindingTarget<IMove, true> target;
    target.m_Target = &ForeignExecute;

    crg::capabilities::CapabilityHandle<IMove> handle;
    handle = &target;
    REQUIRE(handle); // slot is bound

    IMove::Params p{};
    handle(p); // one indirect call — zero virtual dispatch
    REQUIRE(p.m_Speed == 42.f);
}

TEST_CASE("Stage 08 — Muscle CapabilityHandle with null fn ptr reports unbound", "[stage08][ffi]") {
    // An unbound slot (no trampoline registered) must be detectable without a crash.
    crg::capabilities::CapabilityHandle<IMove> handle;
    REQUIRE(!handle);
}

// ─── 3. NodeLink requires C++ construction — cannot be called from C ─────────
// This is the structural proof that Act I (NodeLink) ≠ multi-language.

namespace {
    struct IMockNode {
        virtual ~IMockNode() = default;
    };

    struct MockNodeList : crg::discovery::NodeLink<MockNodeList, IMockNode> {};
}

CRG_DECLARE_UNIVERSAL_NODE_ANCHOR(MockNodeList)

TEST_CASE("Stage 08 — NodeLink self-registration requires C++ constructor (no C-ABI path)", "[stage08][ffi]") {
    // The NodeLink constructor does static_cast<TNode*>(this) — CRTP.
    // This requires:
    //   - C++ object model (vtable installed before the cast is safe)
    //   - C++ runtime (static storage init)
    // A Rust `extern "C"` fn or WASM export cannot call this constructor.
    // The only way to hook a foreign module into NodeLink is via a C++ Proxy Node
    // that wraps the foreign call behind a virtual method.
    //
    // We verify the invariant: static_cast inside NodeLink compiles only because
    // MockNodeList IS_BASE_OF NodeLink<MockNodeList, IMockNode>.
    static_assert(std::is_base_of_v<crg::discovery::NodeLink<MockNodeList, IMockNode>, MockNodeList>,
        "NodeLink CRTP requires C++ inheritance — not expressible in C ABI");
    SUCCEED("NodeLink CRTP self-registration requires C++ — no direct C-ABI path");
}

// ─── 4. Params must be trivially copyable for true C-ABI compatibility ────────

TEST_CASE("Stage 08 — IMove::Params is trivially copyable (POD rule for cross-language safety)", "[stage08][ffi]") {
    // For a Params struct to cross a C or WASM boundary safely:
    //   - no hidden vtable pointer
    //   - no dynamic allocation (std::string, std::vector forbidden)
    //   - layout must be identical on both sides of the boundary
    // std::is_trivially_copyable is the C++ check for this invariant.
    static_assert(std::is_trivially_copyable_v<IMove::Params>,
        "Params must be trivially copyable — no std::string, no std::vector, no vtable");
    SUCCEED("IMove::Params is trivially copyable — safe across C / WASM boundaries");
}
