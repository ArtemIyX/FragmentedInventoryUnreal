# FragmentedInventory Code Review, Round 4

Reviewed: `Plugins/FragmentedInventory` after Round 3 fixes.

Validation: static review only. No build or test command was run. The requester reported the current test suite passes.

## Summary

All findings in this report were fixed after review. The Round 3 full-transfer, exact-move, restriction, candidate-cleanup, replication, and asynchronous asset-readiness fixes remain present and correctly connected.

| Priority | Count |
| --- | ---: |
| P1 | 0 |
| P2 | 0 |

## Resolved lifecycle and ownership

### [P1] Lifecycle: copied inventory instances do not receive creation callbacks
Location: `Source/FragmentedInventory/Public/Components/FragmentedInventoryComponent.h:143-153`, `Source/FragmentedInventory/Private/Components/FragmentedInventoryComponent.cpp:216-298`, `:642-651`
Confidence: High

Issue: `AddItemWithCallback` creates one `FInventoryItemInstance`, invoking fragment `OnItemCreated` on the temporary address, then passes it by const reference to `AddItemWithInstance`. That function copies the instance into a slot and copies it again when `InQuantity` requires multiple slots, changing only the later copies' GUIDs. A partial move to an empty slot similarly copies the source instance, assigns a new GUID, and does not invoke `OnItemCreated` for the new instance.

Impact: Fragments that allocate handles, register runtime state, or bind by item-instance identity never initialize the inventory-owned clone. Each cloned slot still invokes `OnItemDestroyed` when removed, producing unmatched creation and destruction callbacks and invalid external state.

Evidence: `FInventoryItemInstance::InitializeFromDataAsset` invokes `OnItemCreated` at `Source/FragmentedInventory/Private/Data/InventoryItemInstance.cpp:52-53`; only that method invokes it. The copy paths above retain copied `DynamicFragmentData` and create a new `ItemInstanceID` without a corresponding lifecycle call. `Reset` later invokes `OnItemDestroyed` at `:109-127`.

Resolution: `InitializeFromExistingInstance` now creates each inventory-owned clone with a fresh ID, copied dynamic state, and creation callbacks. Callback-based adds defer callbacks until cloning into inventory storage, and partial moves use the same clone path.

### [P1] Lifecycle: reinitialization and component teardown bypass destruction callbacks
Location: `Source/FragmentedInventory/Private/Data/InventorySlotList.cpp:6-20`, `Source/FragmentedInventory/Private/Components/FragmentedInventoryComponent.cpp:30-35`
Confidence: High

Issue: `InitializeSlots` calls `Slots.Empty()` directly. `FInventoryItemInstance` has no destructor that calls `Reset`, so every pre-existing item is discarded without fragment `OnItemDestroyed` callbacks. `EndPlay` clears prediction and async-load state but likewise leaves occupied slots undisposed.

Impact: Calling the public authority `InitializeInventory` on a non-empty inventory, or destroying its owner, leaks fragment-managed registrations and resources. It also violates the established removal contract used by `ClearSlotInternal`.

Evidence: Normal removal reaches `FInventoryItemInstance::Reset` through `ClearSlotInternal` at `Source/FragmentedInventory/Private/Components/FragmentedInventoryComponent.cpp:1042-1057`. `Reset` is the only destruction-callback path at `Source/FragmentedInventory/Private/Data/InventoryItemInstance.cpp:101-133`. Neither bulk-disposal path calls it.

Resolution: `ResetItemInstances` resets valid slots before reinitialization and authoritative component teardown. These bulk paths do not emit normal removal events.

## Resolved transaction ordering and replication

### [P2] Networking: move and swap delegates run before their authoritative revision commits
Location: `Source/FragmentedInventory/Private/Components/FragmentedInventoryComponent.cpp:538-603`, `:665-701`, `:751-770`
Confidence: High

Issue: `MoveItemInternal` and `SwapSlotsInternal` mutate slots and immediately broadcast `OnSlotChanged` and `OnSlotsSwapped`. Their callers commit `InventoryRevision` only after the internal method returns. The same ordering applies to the server RPC path.

Impact: A delegate listener can synchronously issue another inventory mutation while the first transaction's revision is stale. The nested operation commits first, then the outer operation advances the revision again. Observers can receive slot callbacks for a state whose advertised revision has not committed, defeating the revision's transaction-boundary contract and making reentrant Blueprint listeners nondeterministic.

Evidence: Add, remove, clear, restriction, lock, and dynamic-data paths call `CommitAuthorityMutation` before broadcasting their delegates. Move and swap are the exceptions. Multicast delegates are synchronous and Blueprint-callable, so this reentrancy path does not require threading or invalid ownership.

Resolution: move and swap internals now report changed indices. Authority paths commit the revision before broadcasting, while client prediction broadcasts after local prediction state is complete.

## No findings

- Build and module boundaries: public dependencies cover the exposed types; generated-header placement and API exports are valid.
- UObject and reflection safety: persistent UObject references use reflected `TObjectPtr` ownership where required; async completion uses a weak component delegate and releases retained handles on completion or `EndPlay`.
- Authority and replication: mutations are authority guarded, the prediction RPC validates the base revision, rejection dirties both affected Fast Array entries, and shared-observer replication remains `COND_None`.
- Assets and async loading: prediction waits for definition readiness without a synchronous load; completed loads rebroadcast affected slots.
- Round 3 regressions: full empty-slot moves use transfer semantics, occupied-target moves are exact, and restriction mutations validate an occupied item before commit.

## Unverified

- Blueprint fragment implementations that override lifecycle or stack-key events were not exercised in an editor session.
- Dedicated-server and late-join behavior were inspected statically only.
