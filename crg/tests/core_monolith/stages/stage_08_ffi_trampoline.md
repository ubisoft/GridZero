# Stage 08 - FFI Trampoline: the C-ABI Proof

## Problem
Stage 07 showed that a Muscle contract reduces to `void(*)(Params&)` — a pure C-ABI slot. This
stage proves what that buys: which layer of CRG can actually be filled by a foreign language
(Rust, WASM), and which cannot, no matter how decoupled it looks.

## Claim under test
"ABI-neutrality emerges from decoupling — a Rust/WASM module could self-register."
This is **false** for `NodeLink` (auto-registration) and **true** for the Muscle DOD path.
Both are checked here, not asserted.

## Solution - two ABI boundaries, verified separately

```cpp
// Muscle: no virtual -> IsStaticContract == true -> reduces to void(*)(Params&)
struct IMove { struct Params { float m_Speed{ 0.f }; }; };

// Brain: virtual -> IsStaticContract == false -> requires a C++ vtable
struct ILog {
    struct Params { const char* m_Msg{ nullptr }; };
    virtual void Execute(Params&) const = 0;
    virtual ~ILog() = default;
};

// A plain C function satisfies the Muscle slot directly - the exact shape
// a Rust `extern "C" fn` or a WASM export would have.
void ForeignExecute(IMove::Params& p) { p.m_Speed = 42.f; }

crg::capabilities::BindingTarget<IMove, true> target;
target.m_Target = &ForeignExecute;
crg::capabilities::CapabilityHandle<IMove> handle;
handle = &target;
handle(p); // one indirect call through a raw fn ptr - no C++ ABI involved
```

## Why NodeLink cannot make the same claim
`NodeLink`'s constructor performs `static_cast<TNode*>(this)` (CRTP) — it requires the C++ object
model (a vtable already installed) and the C++ static-init runtime. A foreign `extern "C"`
function cannot call a C++ constructor, so it cannot self-register into a `NodeLink` chain
directly. The only bridge is a C++ Proxy Node that wraps the foreign call behind a virtual
method — which reintroduces the C++ ABI at the wrapper, not the foreign side.

## The trivially-copyable rule
For `Params` to cross a C or WASM boundary safely it must have no hidden vtable pointer, no
dynamic allocation (`std::string`, `std::vector` are forbidden in that struct), and identical
layout on both sides. `std::is_trivially_copyable_v<Params>` is the compile-time check for this.

## Narrative consequence
- The auto-registration mechanism buys **C++ binary stability** (e.g. MSVC and Clang linking
  into the same process) — not multi-language interop.
- The Muscle contract shape buys **pure C-ABI** — true multi-language interop, as a structural
  consequence of the contract having no virtual methods, not as a feature bolted on afterward.

## When to use
- You need to reason precisely about which CRG layer a foreign-language module can actually
  plug into, instead of assuming "decoupled" implies "language-agnostic" everywhere.

## What's new vs Stage 07
Same Muscle contract shape as Stage 07. This stage adds the cross-language angle: a plain C
function pointer (standing in for a Rust/WASM export) is shown filling the slot directly, and
`NodeLink`'s C++-only registration path is checked as the boundary that does *not* extend to
foreign code.
