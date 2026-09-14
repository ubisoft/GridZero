# model_shell_traits — design notes

Three independent customization/dispatch points for `ModelShell<TDomain>`
(`model_shell.hpp`), plus the return-type utilities `Invoke`/`TryInvoke` need.

## ModelShellTraits — buffer sizing

`MaxSize`/`MaxAlignment` size and align the shell's fixed-size buffer. A
domain widens its buffer or raises alignment by specializing
`ModelShellTraits<TDomain>` — every `static_assert` that checks a model
against `StorageSize`/`MaxAlignment` lives in `model_shell.hpp`, not here;
this header only supplies the defaults.

## ModelShellMutabilityTraits — opt-in mutability

Defaults to `IsMutable = false`: a domain's models are immutable through
`ModelShell` unless the domain specializes this trait to `true`, which
unlocks `Mutate()`/`TryMutate()` (both gated behind
`std::enable_if_t<ModelShellMutabilityTraits<TDomain>::IsMutable>` in
`model_shell.hpp`). This is a deliberate default, not an oversight — most
domains route through immutable snapshots, so opting in is a conscious
per-domain decision.

## ModelShellMethodTraits — const-only method extraction

The primary template `static_assert`s unconditionally
(`always_false_v<TFunc>`) with a message naming the actual constraint:
routed interface methods must be `const`. Only the partial specialization
matching `R (I::*)(const ModelShell<TDomain>&, Args...) const` compiles,
extracting `Interface`/`ReturnType` for `Invoke`/`TryInvoke`. A non-const
method pointer, or one whose first parameter isn't
`const ModelShell<TDomain>&`, fails to match the specialization and falls
through to the primary template's `static_assert` — the mismatch is
reported as the stated architecture rule, not a generic "no matching
specialization" error.

## IsOptional / TryInvokeResult_t — TryInvoke's return-type massaging

`TryInvoke` must return "nothing happened" without forcing every interface
to declare its return type as `std::optional<T>` itself. `TryInvokeResult_t<R>`:

- `R = void` → `void` (nothing to report; `TryInvoke` just doesn't call).
- `R` already `std::optional<U>` → `R` unchanged (`IsOptional` detects this
  via `IsOptionalImpl`'s partial specialization) — avoids the
  double-wrapped `std::optional<std::optional<U>>` that a naive
  `std::optional<R>` would produce.
- anything else → `std::optional<R>`.

## Include-cycle note

`ModelShell<TDomain>` is forward-declared here rather than included: this
header is included *by* `model_shell.hpp` (for `ModelShellTraits` and the
`alignas` it drives), so a real include back would be circular.
