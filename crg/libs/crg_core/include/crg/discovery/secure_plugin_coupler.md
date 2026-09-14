# SecurePluginCoupler

Validates an untrusted intrusive singly-linked list before it is spliced onto host-visible storage.

## Algorithm

Floyd's tortoise-and-hare cycle detection: two pointers advance at speeds 1 and 2 through the list. If they meet, a cycle exists. O(n) time, O(1) space — safe to call on untrusted plugin data.

## TNode contract

`TNode` must expose a `TNode* m_Next` field. No other requirements.

## Usage context

Called by `DomainSynchronizer::OnPluginLoad` to reject a malformed plugin chain before splicing it onto the host. A cycle in the plugin chain would turn the splice and unsplice walks into infinite work on host-visible storage.

## Result semantics

`nodeCount` is set to 0 when `hasCycle` is true — the walk is aborted early and no count is available.
