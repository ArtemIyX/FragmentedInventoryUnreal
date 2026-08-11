# FragmentedInventory Code Review, Round 9

Reviewed: `Plugins/FragmentedInventory` after Round 8 fixes.

Validation: static review only. No build or test command was run. The requester reported the current test suite passes.

## Summary

One P2 public API defect remains. The Blueprint-visible definition-only acceptance query can report success for a conditional stack that the actual add operation rejects.

| Priority | Count |
| --- | ---: |
| P2 | 1 |

## Runtime contracts

### [P2] Correctness: Blueprint acceptance query ignores conditional stack keys
Location: `Source/FragmentedInventory/Private/Data/InventorySlot.cpp:32-71`, `Source/FragmentedInventory/Private/Libs/FragmentedInventoryLib.cpp:13-17`, `Source/FragmentedInventory/Private/Components/FragmentedInventoryComponent.cpp:333-356`
Confidence: High

Issue: `UFragmentedInventoryLib::CanAcceptItem` exposes `FInventorySlot::CanAcceptItem` to Blueprints with only an item definition and quantity. For an occupied slot, that function checks definition equality and remaining capacity but cannot evaluate `CanStackWith` or the incoming instance's stack key. A conditional stack therefore reports that it accepts the definition even when a newly created instance cannot merge.

Impact: UI or gameplay code can enable an add/drop action from the Blueprint query, then have `AddItemToSlot` reject the same operation. Custom stack-key policies exhibit the same mismatch.

Evidence: `AddItemToSlot` first calls `CanAcceptItem`, then creates a candidate and separately calls `CanStackWith`. The public Blueprint library exposes only the first check. Round 8 added an instance-aware slot search but did not add an instance-aware acceptance query.

Fix direction: Add `CanAcceptItemInstance(const FInventorySlot&, const FInventoryItemInstance&, int32)` to the Blueprint library and slot type. It should check `CanPlaceItem` for empty slots and `CanStackWith` plus quantity for occupied slots. Document the existing definition-only query as an empty-slot capacity check, or remove it from the Blueprint surface.

Resolution: Added the instance-aware query, made the legacy definition-only Blueprint query conservative and deprecated for occupied-stack checks, and routed `AddItemToSlot` through the instance-aware contract. The same audit found provisional merge candidates ran lifecycle callbacks despite never becoming stored items; candidates now delay creation callbacks until committed and invoke no destruction callback when discarded. Automation coverage verifies conditional acceptance and provisional lifecycle behavior.

## No findings

- Build and module boundaries: runtime dependencies, exports, and generated-header placement are valid.
- UObject and reflection safety: instanced fragments, soft references, and transient caches have suitable ownership.
- Lifecycle and cleanup: Round 7 indexed teardown preserves per-fragment dynamic data.
- Networking and prediction: server validation, Fast Array corrections, and lifecycle callback suppression remain coherent.
- Async loading and performance: loading uses weak callbacks and inventory mutations are event-driven.

## Unverified

- Editor validation and cooked legacy assets were not exercised.
- Dedicated-server, late-join, and client/server prediction flows were inspected statically only.
