// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/InventorySlotList.h"
#include "Data/ItemDefinitionAsset.h"
#include "FragmentedInventoryComponent.generated.h"

struct FStreamableHandle;

// Delegate signatures
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSlotChanged, int32, SlotIndex, const FInventorySlot&, Slot);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnItemAdded, int32, SlotIndex, const UItemDefinitionAsset*,
                                               ItemDataAsset, int32, Quantity);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnItemRemoved, int32, SlotIndex, const UItemDefinitionAsset*,
                                               ItemDataAsset, int32, Quantity);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSlotsSwapped, int32, SlotIndexA, int32, SlotIndexB);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FRAGMENTEDINVENTORY_API UFragmentedInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFragmentedInventoryComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

private:
	/** @brief Broadcasts a changed slot after its mutation is complete. */
	void BroadcastSlotChanged(int32 InSlotIndex);

	/** @brief Broadcasts committed slot changes in mutation order. */
	void BroadcastSlotChanges(const TArray<int32>& InSlotIndices);

	/** @brief Marks an authoritative slot change for Fast Array and push-model replication. */
	void MarkSlotDirty(FInventorySlot& InSlot);

	/** @brief Commits an authoritative inventory transaction and advances its revision. */
	void CommitAuthorityMutation();

	/** @brief Forces current server slot state to replicate after a rejected prediction. */
	void ForceAuthoritativeSlotRefresh(int32 InSlotIndex);

	/** @brief Clears a slot without broadcasting item removal. */
	bool ClearSlotInternal(int32 InSlotIndex, const UItemDefinitionAsset*& OutItemDataAsset, int32& OutQuantity,
		bool bInInvokeDestroyedCallbacks = true);

	/** @brief Finds an empty slot that accepts an item definition. */
	int32 FindFirstEmptySlotForItem(const UItemDefinitionAsset* InItemDataAsset) const;

	/** @brief Executes a move without changing the inventory revision. */
	bool MoveItemInternal(int32 InFromSlotIndex, int32 InToSlotIndex, int32 InQuantity,
		TArray<int32>* OutChangedSlotIndices = nullptr, bool* bOutSlotsSwapped = nullptr);

	/** @brief Executes a swap without changing the inventory revision. */
	bool SwapSlotsInternal(int32 InSlotIndexA, int32 InSlotIndexB, TArray<int32>* OutChangedSlotIndices = nullptr);

	/** @brief Starts a non-blocking load for a replicated item definition. */
	void EnsureItemDefinitionLoaded(const FInventoryItemInstance& InItemInstance);

	/** @brief Checks whether each item definition needed by a predicted move is loaded. */
	bool AreMoveItemDefinitionsLoaded(int32 InFromSlotIndex, int32 InToSlotIndex);

	/** @brief Restores the local state captured before the pending prediction. */
	void RollbackPendingMove();

	/** @brief Clears a resolved prediction after its authoritative slot refresh. */
	void ResolvePendingMoveIfRefreshed(int32 InSlotIndex);

	FInventorySlot* GetSlotMutable(int32 InSlotIndex);

	// Create a new item instance
	FInventoryItemInstance CreateItemInstance(const UItemDefinitionAsset* InItemDataAsset,
		bool bInInvokeCreatedCallbacks = true) const;

	struct FPendingMovePrediction
	{
		int32 PredictionId = INDEX_NONE;

		int32 FromSlotIndex = INDEX_NONE;

		int32 ToSlotIndex = INDEX_NONE;

		FInventorySlot FromSlotBefore;

		FInventorySlot ToSlotBefore;

		bool bAwaitingFromSlotRefresh = false;

		bool bAwaitingToSlotRefresh = false;
	};

public:
	/** @brief Slot count created on authority during BeginPlay. Zero creates an empty inventory. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "0", ToolTip = "Slot count created on authority during BeginPlay. Zero creates an empty inventory."))
	int32 DefaultSlotCount;

	/** @brief Slot type assigned during automatic initialization. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (ToolTip = "Slot type assigned during automatic initialization."))
	EInventorySlotType DefaultSlotType;

	/** @brief When true, authority initializes slots during BeginPlay. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (ToolTip = "When true, authority initializes slots during BeginPlay."))
	bool bAutoInitialize;

	/** @brief Uses push-model replication. This must match for every instance of the component class. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (ToolTip = "Uses push-model replication. This must match for every instance of the component class."))
	bool bUseNetworkPushModel;

protected:
	// The actual inventory data
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Inventory")
	FInventorySlotList SlotList;

	/** @brief Monotonic server transaction revision used to reject stale predicted moves. */
	UPROPERTY(ReplicatedUsing = OnRep_InventoryRevision, BlueprintReadOnly, Category = "Inventory")
	int32 InventoryRevision;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_InventoryRevision();

public:
	// Initialize inventory with a specific number of slots
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void InitializeInventory(int32 InSlotCount, EInventorySlotType InDefaultSlotType = EInventorySlotType::General);

	// Add item to inventory (finds best slot automatically)
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool AddItem(int32& OutSlotIndex, const UItemDefinitionAsset* InItemDataAsset, int32 InQuantity = 1);

	// Add item with pre-configured instance (for items with custom dynamic data like durability)
	// Note: This will NOT stack with existing items, as each instance is unique
	bool AddItemWithInstance(int32& OutSlotIndex, const FInventoryItemInstance& InItemInstance, int32 InQuantity = 1);

	// Add item with callback to configure dynamic data after creation
	// Callback signature: void(FInventoryItemInstance& OutInstance)
	template <typename CallbackType>
	bool AddItemWithCallback(int32& OutSlotIndex, const UItemDefinitionAsset* InItemDataAsset, int32 InQuantity,
	                         CallbackType&& InConfigureCallback)
	{
		OutSlotIndex = INDEX_NONE;
		if (GetOwnerRole() != ROLE_Authority || !IsValid(InItemDataAsset) || InQuantity <= 0)
		{
			return false;
		}

		FInventoryItemInstance ItemInstance = CreateItemInstance(InItemDataAsset, false);
		InConfigureCallback(ItemInstance);

		return AddItemWithInstance(OutSlotIndex, ItemInstance, InQuantity);
	}

	// Add item to specific slot
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool AddItemToSlot(int32 InSlotIndex, const UItemDefinitionAsset* InItemDataAsset, int32 InQuantity = 1);

	// Add item to specific slot with pre-configured instance
	bool AddItemToSlotWithInstance(int32 InSlotIndex, const FInventoryItemInstance& InItemInstance,
	                               int32 InQuantity = 1);

	// Remove item from inventory (removes from first found slot)
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool RemoveItem(const UItemDefinitionAsset* InItemDataAsset, int32 InQuantity = 1);

	// Remove item from specific slot
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool RemoveItemFromSlot(int32 InSlotIndex, int32 InQuantity = 1);

	// Clear a slot completely
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void ClearSlot(int32 InSlotIndex);

	// Swap two slots
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool SwapSlots(int32 InSlotIndexA, int32 InSlotIndexB);

	// Move item from one slot to another (handles stacking)
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool MoveItem(int32 InFromSlotIndex, int32 InToSlotIndex, int32 InQuantity = -1);

	/** @brief Predicts a locally owned move and submits it for authoritative validation. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Networking", meta = (ToolTip = "Predicts one locally owned move and submits it to the server. Additional predictions wait for authoritative reconciliation."))
	bool RequestMoveItem(int32 InFromSlotIndex, int32 InToSlotIndex, int32 InQuantity = -1);

	/** @brief Replaces one fragment's runtime data on authority and marks its slot for replication. */
	bool SetSlotItemFragmentDynamicData(int32 InSlotIndex, TSubclassOf<UItemFragment_Base> InFragmentClass,
		const FInstancedStruct& InDynamicData);

	// Get slot by index
	UFUNCTION(BlueprintCallable, Category = "Inventory", BlueprintPure)
	const FInventorySlot& GetSlot(int32 InSlotIndex) const;

	// Check if slot is valid and exists
	UFUNCTION(BlueprintCallable, Category = "Inventory", BlueprintPure)
	bool IsValidSlot(int32 InSlotIndex) const;

	// Get number of slots
	UFUNCTION(BlueprintCallable, Category = "Inventory", BlueprintPure)
	int32 GetSlotCount() const { return SlotList.GetSlotCount(); }

	// Find first empty slot
	UFUNCTION(BlueprintCallable, Category = "Inventory", BlueprintPure)
	int32 FindFirstEmptySlot(EInventorySlotType InSlotType) const;

	// Find first empty slot
	UFUNCTION(BlueprintCallable, Category = "Inventory", BlueprintPure)
	int32 FindFirstEmptySlotAnyType() const;


	// Count how many of an item we have
	UFUNCTION(BlueprintCallable, Category = "Inventory", BlueprintPure)
	int32 CountItem(const UItemDefinitionAsset* InItemDataAsset) const;

	// Check if we have enough of an item
	UFUNCTION(BlueprintCallable, Category = "Inventory", BlueprintPure)
	bool HasItem(const UItemDefinitionAsset* InItemDataAsset, int32 InQuantity = 1) const;

	// Get all slots
	const FInventorySlotList& GetSlotList() const { return SlotList; }

	// Set slot type
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetSlotType(int32 InSlotIndex, EInventorySlotType InSlotType);

	/** @brief Updates restrictions when the current item remains valid. Returns false on non-authority, invalid slot, or incompatible item. */
	UFUNCTION(BlueprintCallable, Category = "Inventory", meta = (ToolTip = "Updates restrictions only if the current item remains valid. Returns false on non-authority, an invalid slot, or an incompatible item."))
	bool SetSlotRestrictionTags(int32 InSlotIndex, const FGameplayTagContainer& InRestrictionTags);

	// Lock/unlock a slot
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetSlotLocked(int32 InSlotIndex, bool bInLocked);

	// Get fragment dynamic data from item in slot
	template <typename T>
	const T* GetSlotItemFragmentDynamicData(int32 InSlotIndex, TSubclassOf<UItemFragment_Base> InFragmentClass) const
	{
		const FInventorySlot* slot = SlotList.GetSlot(InSlotIndex);
		if (slot == nullptr || slot->IsEmpty())
		{
			return nullptr;
		}

		return slot->ItemInstance.GetFragmentDynamicData<T>(InFragmentClass);
	}

	/** @brief Handles an authoritative Fast Array slot update on this machine. */
	void HandleReplicatedSlotChange(int32 InSlotIndex, const FInventorySlot& InSlot);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory")
	int32 GetTotalSlotCount() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory")
	int32 GetUsedSlotCount() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory")
	int32 GetEmptySlotCount() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Inventory")
	float GetInventoryUsagePercent() const;

public:
	// Delegates
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnSlotChanged OnSlotChanged;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnItemAdded OnItemAdded;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnItemRemoved OnItemRemoved;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnSlotsSwapped OnSlotsSwapped;

protected:
	UFUNCTION(Server, Reliable)
	void ServerRequestMoveItem(int32 InPredictionId, int32 InBaseRevision, int32 InFromSlotIndex, int32 InToSlotIndex, int32 InQuantity);

	UFUNCTION(Client, Reliable)
	void ClientAcknowledgeMoveItem(int32 InPredictionId, int32 InServerRevision);

	UFUNCTION(Client, Reliable)
	void ClientRejectMoveItem(int32 InPredictionId, int32 InServerRevision);

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FFragmentedInventoryPredictionRollbackTest;
	friend class FFragmentedInventoryPredictionLifecycleRollbackTest;
	friend class FFragmentedInventoryPredictionMergeLifecycleRollbackTest;
#endif

	int32 NextPredictionId;

	int32 ClientKnownInventoryRevision;

	TOptional<FPendingMovePrediction> PendingMovePrediction;

	TMap<FSoftObjectPath, TSharedPtr<FStreamableHandle>> PendingItemDefinitionLoads;
};
