# FragmentedInventory Code Review, Round 8

Reviewed: `Plugins/FragmentedInventory` after Round 7 fixes.

Validation: static review only. No build or test command was run. The requester reported the current test suite passes.

## Summary

All Round 8 findings are resolved. Definition-only slot queries now return only empty compatible slots, while the new instance-aware query evaluates conditional stack compatibility.

| Priority | Count |
| --- | ---: |
| P2 | 0 |

## Runtime contracts

### [Resolved P2] Correctness: definition-only slot search ignores conditional stack compatibility
Location: `Source/FragmentedInventory/Private/Data/InventorySlotList.cpp:87-115`, `Source/FragmentedInventory/Private/Components/FragmentedInventoryComponent.cpp:343-356`
Confidence: High

Previous issue: `FInventorySlotList::FindSlotForItem` returned an occupied slot when `CanAcceptItem` accepted the item definition and quantity. `CanAcceptItem` compares only the definition asset and capacity. It did not call `CanStackWith`, so it could not evaluate the item instance's stack key. Conditional stack fragments could therefore cause the helper to return an occupied stack that the real add path rejected.

Previous impact: C++ consumers of the documented `FindSlotForItem` helper could select an invalid destination, then fail when attempting the add or move. This was observable for items with `UItemFragment_StackableConditional` or any custom instance-dependent stack policy.

Evidence: `AddItemToSlot` creates a candidate instance and calls `Slot->CanStackWith(CandidateItemInstance)` after the definition-only acceptance check. The former `FindSlotForItem` had no instance parameter and performed no equivalent check. The helper was public and documented as finding a stackable or empty slot.

Resolution: The definition-only query now searches empty slots only. `FindSlotForItemInstance` accepts a specific instance and uses `CanStackWith` for occupied slots. Regression coverage verifies that conditional stacks are skipped.

## No findings

- Build and module boundaries: runtime dependencies, API exports, and generated-header placement are valid.
- UObject and reflection safety: instanced fragment ownership, transient caches, and Fast Array ownership are correctly declared.
- Lifecycle and cleanup: Round 7 array-index teardown correctly preserves fragment dynamic-data identity.
- Networking and prediction: authority checks, revision validation, rejection refresh, and lifecycle suppression are connected correctly.
- Async loading and performance: loads are weakly bound and mutation replication is slot-granular.

## Unverified

- Editor validation and cooked assets with legacy overlapping fragment classes were not exercised.
- Dedicated-server, late-join, and client/server prediction flows were inspected statically only.
