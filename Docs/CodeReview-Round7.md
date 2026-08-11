# FragmentedInventory Code Review, Round 7

Reviewed: `Plugins/FragmentedInventory` after Round 6 fixes.

Validation: static review only. No build or test command was run. The requester reported the current test suite passes.

## Summary

All Round 7 findings are resolved. Item teardown now uses the fragment array index that created each dynamic-data entry, and editor validation rejects overlapping base-and-derived fragment classes.

| Priority | Count |
| --- | ---: |
| P2 | 0 |

## Lifecycle and fragment data

### [Resolved P2] Lifecycle: polymorphic fragments resolve the same dynamic-data entry during destruction
Location: `Source/FragmentedInventory/Private/Data/InventoryItemInstance.cpp:166-183`, `Source/FragmentedInventory/Private/Data/ItemDefinitionAsset.cpp:121-128`, `:35-53`
Confidence: High

Previous issue: `FInventoryItemInstance::Reset` iterated every fragment but retrieved its dynamic-data index through `GetFragmentIndex(Fragment->GetClass())`. `GetFragmentIndex` returns the first `IsA` match, while data validation rejected only exact duplicate classes. An item definition could therefore contain `UItemFragment_StackableConditional` followed by `UItemFragment_Stackable`. Reset found index zero for both fragments because the conditional fragment is also a stackable fragment.

Previous impact: The base stackable fragment received `OnItemDestroyed` with the conditional fragment's dynamic data, while its own dynamic data received no destruction callback. Custom fragment pairs with different dynamic structs or teardown side effects could corrupt cleanup state or invoke teardown against incompatible data.

Evidence: Instance creation stores dynamic data by the `Fragments` array position. The former reset path discarded that position and performed a polymorphic lookup. The previous validation permitted related fragment classes because it stored only exact `UClass*` values in `FragmentClasses`.

Resolution: Reset now uses each fragment's array index for dynamic data. Validation now rejects overlapping fragment inheritance. A regression test verifies that base and derived lifecycle fragments each receive matching destruction data.

## No findings

- Build and module boundaries: runtime dependencies, API exports, and generated-header placement are valid.
- UObject and reflection safety: replicated item state, transient asset caches, and instanced fragment ownership are correctly declared.
- Networking and prediction: server validation, Fast Array refresh, push-model marking, and Round 6 lifecycle suppression are connected correctly.
- Async loading: replicated definitions load asynchronously and handles are released during teardown.
- Performance: inventory mutations are event-driven and dirty only the affected Fast Array entries.

## Unverified

- Editor validation was not exercised with a base-and-derived fragment pair.
- Client/server and late-join flows were inspected statically only.
