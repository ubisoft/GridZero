# model_node — Design Notes

`ModelNode<TDomain, THash>` is a zero-state `NodeLink` instance emitted once per
(Domain, Model) by `CRG_DECLARE_DOMAIN_MODELS`.

## Slot identity invariant

The **slot** of a model equals the *position* of its first `ModelNode` in the
linearized NodeLink chain. This is the ordering that `DenseIDStore<TDomain>::Refresh`
walks at each `DomainSynchronizer` tick. Consequence: chain order must be stable and
append-only; no reordering is permitted after any node has been observed.

## Constraints

- **Dedup'd, append-only, single-thread.** The chain is walked on the
  `DomainSynchronizer` tick thread only. No locking, no concurrent mutation.
- **No `Intern` path, no mutable singleton.** Registration happens purely via the
  static-initializer of each `ModelNode` instance — zero explicit `Init()` calls.
- **No race surface.** All nodes are constructed before `main` enters; the chain is
  read-only by the time any tick fires.
