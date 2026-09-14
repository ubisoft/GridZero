# node_link — design notes

## Include ordering constraint

`universal_anchor.hpp` is `#include`d at the **bottom** of this file, after `NodeLink` and
`CRG_MARK_INTERFACE_BOOTSTRAP` are fully defined. This is intentional:

- In plugin mode, `universal_anchor_plugins.inl` (pulled in transitively) re-enters this
  translation unit via an `#include` cycle. `NodeLink` must already be defined at that point.
- In monolithic mode, GCC requires `UniversalAnchor` to be a complete type at the point any
  static member function of `NodeLink` (e.g. `RefreshCache`, `GetCache`) is first named.
  Placing the include at the top would break this in either mode.

Do not move the `#include "crg/discovery/universal_anchor.hpp"` directive.

## `NodeLinkTraits` — optional cache ordering

`NodeLinkTraits<TNode>` is the traits customization point for post-registration cache ordering. The default specialization is a no-op inlined away by the compiler.

To define a sort order, specialize in the `.cpp` where your static instances live (Mutual Anonymity: the node header stays clean):

```cpp
// my_nodes.cpp — co-located with the static const instances
template<>
struct crg::discovery::NodeLinkTraits<IGreeterNode> {
    static void SortCache(std::vector<const IGreeterNode*>& cache) {
        std::sort(cache.begin(), cache.end(),
            [](const IGreeterNode* a, const IGreeterNode* b) {
                return a->m_Priority < b->m_Priority;
            });
    }
};

// Force instantiation here so the specialization is visible when RefreshCache() fires.
template class crg::discovery::NodeLink<IGreeterNode, IGreeter>;
```

`SortCache` is called once at the end of every `RefreshCache()` — both the initial call from `Linearizer()` and every explicit `TNode::RefreshCache()`. Works for any `NodeLink` chain: capability populators, decoupled plugin lists, scheduler task chains.

## `static_cast` macro hazard

Some host environments (notably certain engine SDKs) redefine `static_cast` as a macro that
performs a `dynamic_cast`-based RTTI check. Inside a base-class constructor the most-derived
vtable is not yet installed, so that RTTI check fails spuriously on the otherwise well-defined
downcast `static_cast<TNode*>(this)`.

The constructor suspends the macro with `#pragma push_macro("static_cast")` / `#undef static_cast`
and restores it with `#pragma pop_macro("static_cast")`. Both pragmas are no-ops when the macro is
not defined, so the code is safe in standard toolchains.
