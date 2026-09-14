# TypeList

`TypeList<TTypes...>` is a lightweight, zero-cost compile-time container of types.
It carries no data and no vtable — it exists only as a template parameter pack wrapper.

Primary use: mass-registering a set of Models to a set of Capabilities in a single declaration, avoiding repeated boilerplate per type.
