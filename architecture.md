# GridZero Architecture

## Core idea

A capability is a function (or object) associated with a model type and an optional
set of context axes. The routing tensor maps `(model_dense_id, axis_0, axis_1, ...)` to
a function pointer or virtual pointer. Finding a capability is one array read; invoking
it is one indirect call. No allocation, no lock, no branch on the hot path.

## Layer 1 — Discovery (NodeLink)

`NodeLink<TNode, TContract>` is an intrusive singly-linked chain. Each implementation
declares a `static const` instance of its node type; the `NodeLink` constructor appends
it to a process-wide chain at static-init time. The caller calls `TNode::Visit(lambda)`
— it never knows how many nodes exist or where they are defined.

This is the only mechanism that crosses translation-unit boundaries in CRG. There is
no global map, no `Init()` function, and no include coupling between implementors and
callers.

**Optional cache ordering.** `NodeLinkTraits<TNode>` is the traits customization point for post-registration ordering. Specialize it in the `.cpp` where your static instances live; the default is a no-op with zero overhead. Works for any chain — capability populators, bare plugin lists, scheduler chains. See `node_link.md` for the explicit-instantiation pattern required to guarantee visibility.

**Plugin variant.** When `CRG_PLUGINS_ENABLED=1`, `PluginLocalNode` uses a
per-binary head (`PluginLocalStorage<T>::s_LocalHead`) instead of the process-wide
chain. `CRG_DEFINE_PLUGIN(Name)` generates three C ABI entry points (`CRG_Bootstrap`,
`CRG_Shutdown`, `CRG_BridgeInit`) that splice the plugin's local chains into the host's
chains and unsplice them on unload. The host loader decides which entry point to call —
the macro emits all three. There is no separate "bridge" vs "plugin" distinction for
user code.

## Layer 2 — Models (DenseID)

A domain is a tag type. `CRG_DECLARE_DOMAIN(D)` registers a TypeHash for it.
`CRG_DECLARE_DOMAIN_MODELS(D, M1, M2, ...)` registers models into that domain.
`DenseIndexStore<D>` assigns each model a compact integer slot (DenseID). Slots
are assigned on first access and never change — the cache is append-only.

`ModelToken<D>` is a thin wrapper around a DenseID. It is the key passed to
`CapabilityRouter::Find`. An invalid token (default-constructed, or from an
unknown hash) returns an empty handle from `Find` — no crash, no assertion.

## Layer 3 — Capabilities (routing tensor)

A **Muscle** (DOD) contract is a plain struct with a nested `Params` struct and no
virtual methods. A **Brain** (OOP) contract has virtual methods.

`CapabilityRoutingTraits<D, C>` selects the tensor shape for contract `C` in domain
`D`. The default is a 0-D tensor (one cell per model). Specializing `SpaceType` to
`CapabilitySpace<Axis1, Axis2, ...>` adds dimensions; the offset is computed by
Horner's method with no branches.

`CapabilityBinding<D, M, Template>` is a static instance in an anonymous namespace
that registers the capability `Template<M, At<coord>>` into the tensor. It does not
compile or instantiate anything at the call site — only at the definition site.

`CapabilityRouter<D>::Find<C>(handle, axes...)` returns a `CapabilityHandle<C>`.
For Muscle contracts, `operator()` calls `Execute(Params&)` directly (one function
pointer). For Brain contracts, `operator->` returns the virtual interface pointer.

## Layer 4 — Type erasure (ModelShell)

`ModelShell<D>` erases the concrete model type into a 64-byte fixed-size buffer
while keeping the DenseID for router lookups. Unlike classic SBO, there is no
heap fallback: an oversized model is rejected at compile time via `static_assert`.
`shell.Invoke<&IFoo::Method>(args...)` routes through the tensor and dispatches
via virtual call in a single expression.
`TryInvoke` returns `std::optional<R>` and returns `{}` when no binding is found.

## Plugin system

### The single macro

`CRG_DEFINE_PLUGIN(Name)` is the only macro a plugin author needs. It emits three C-ABI entry points:

- `CRG_Bootstrap(CRG_Context*)` — splice local chains into the host, fire `OnPluginLoad` on each `ISyncNode`.
- `CRG_Shutdown()` — unsplice chains, fire `OnPluginUnload`.
- `CRG_BridgeInit(CRG_Context*)` — alias for `CRG_Bootstrap`; called by the managed loader for bridge-loaded plugins.

The host loader decides which to call. User code declares one macro regardless of load path.

### Decoupled plugins (no Domain, no Capability)

A plugin can expose a plain `NodeLink` chain with no domain and no capability routing. The chain is spliced and unspliced via the same `CRG_Bootstrap`/`CRG_Shutdown` protocol. `DomainSynchronizer` is not involved. Example: a list of `IGreeterNode` plugins discovered across DLLs.

### Capability plugins (with Domain)

When a plugin provides `CapabilityBinding` instances, the chain splice also triggers `DomainSynchronizer<TDomain>::OnPluginLoad`, which rebuilds the routing tensor for that domain in the host's storage. The host holds the `TensorArenaStorage`; the cross-binary `UniversalAnchor` redirects writes from the plugin into the host arena during the bootstrap window.

### Bridge scenario

A "bridge" DLL is loaded by a managed loader that calls `CRG_BridgeInit` instead of `CRG_Bootstrap`. Because `CRG_DEFINE_PLUGIN` emits `CRG_BridgeInit` as an alias, no separate macro or file is needed. The plugin author writes `CRG_DEFINE_PLUGIN(Name)` and the loader picks the right entry point.

### Manual loading surface

```cpp
// 1. Build context
crg::host::HostDescriptor hostDesc;
hostDesc.m_HostAnchorsHead = crg::discovery::AnchorLink::GetLocalHead();
crg::host::CRG_Context ctx{ &hostDesc };

// 2. Load and bootstrap (standard path)
HMODULE h = LoadLibraryA(path);
((void(*)(crg::host::CRG_Context*))GetProcAddress(h, "CRG_Bootstrap"))(&ctx);
// → Splice, DomainSynchronizer::OnPluginLoad, RefreshCache — all automatic.

// 3. Use
auto gate = CapabilityRouter<D>::Find<C>(handle);
gate(params);

// 4. Unload
((void(*)())GetProcAddress(h, "CRG_Shutdown"))();
FreeLibrary(h);
// → Unsplice, ClosePortal, RefreshCache — all automatic.
```

Zero GridZero `.cpp` required in the host or the plugin.

### Security

`SecurePluginCoupler` runs Floyd's cycle detection on the plugin's chain before any splice. A cycle in an untrusted binary would turn the host-side walk into infinite work — it is rejected with zero splice applied.

## Invariants

- `crg_core` is header-only. No `.cpp` is ever compiled from it.
- No `#ifdef _WIN32` or OS-specific header in `crg_core` headers.
- All routing offsets are computed at compile time or by Horner's method — no
  `std::unordered_map` or `std::map` on the hot path.
- DenseID slots are stable once assigned. `Refresh()` is append-only.
- `CRG_PLUGINS_ENABLED=0` and `CRG_PLUGINS_ENABLED=1` must never be mixed in
  the same binary (ODR violation on static-inline members).
