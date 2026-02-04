// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/InventorySlotList.h"
#include "Data/ItemDefinitionAsset.h"
#include "FragmentedInventoryComponent.generated.h"

// Delegate signatures
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSlotChanged, int32, SlotIndex, const FInventorySlot&, Slot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnItemAdded, int32, SlotIndex, const UItemDefinitionAsset*, ItemDataAsset, int32, Quantity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnItemRemoved, int32, SlotIndex, const UItemDefinitionAsset*, ItemDataAsset, int32, Quantity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSlotsSwapped, int32, SlotIndexA, int32, SlotIndexB);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FRAGMENTEDINVENTORY_API UFragmentedInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFragmentedInventoryComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

private:
	// Internal helpers
	void BroadcastSlotChanged(int32 InSlotIndex);
	FInventorySlot* GetSlotMutable(int32 InSlotIndex);
	
	// Create a new item instance
	FInventoryItemInstance CreateItemInstance(const UItemDefinitionAsset* InItemDataAsset) const;
protected:
	// Default number of slots to initialize
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
	int32 DefaultSlotCount;

	// Default slot type for initialization
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
	EInventorySlotType DefaultSlotType;

	// Whether to auto-initialize on begin play
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
	bool bAutoInitialize;

protected:
	// The actual inventory data
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Inventory")
	FInventorySlotList SlotList;

	

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	// Initialize inventory with a specific number of slots
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void InitializeInventory(int32 InSlotCount, EInventorySlotType InDefaultSlotType = EInventorySlotType::General);

	// Add item to inventory (finds best slot automatically)
	UFUNCTION(BlueprintCallable, Category = "Inventory", BlueprintAuthorityOnly)
	bool AddItem(int32& OutSlotIndex, const UItemDefinitionAsset* InItemDataAsset, int32 InQuantity = 1);

	// Add item to specific slot
	UFUNCTION(BlueprintCallable, Category = "Inventory", BlueprintAuthorityOnly)
	bool AddItemToSlot(int32 InSlotIndex, const UItemDefinitionAsset* InItemDataAsset, int32 InQuantity = 1);

	// Remove item from inventory (removes from first found slot)
	UFUNCTION(BlueprintCallable, Category = "Inventory", BlueprintAuthorityOnly)
	bool RemoveItem(const UItemDefinitionAsset* InItemDataAsset, int32 InQuantity = 1);

	// Remove item from specific slot
	UFUNCTION(BlueprintCallable, Category = "Inventory", BlueprintAuthorityOnly)
	bool RemoveItemFromSlot(int32 InSlotIndex, int32 InQuantity = 1);

	// Clear a slot completely
	UFUNCTION(BlueprintCallable, Category = "Inventory", BlueprintAuthorityOnly)
	void ClearSlot(int32 InSlotIndex);

	// Swap two slots
	UFUNCTION(BlueprintCallable, Category = "Inventory", BlueprintAuthorityOnly)
	bool SwapSlots(int32 InSlotIndexA, int32 InSlotIndexB);

	// Move item from one slot to another (handles stacking)
	UFUNCTION(BlueprintCallable, Category = "Inventory", BlueprintAuthorityOnly)
	bool MoveItem(int32 InFromSlotIndex, int32 InToSlotIndex, int32 InQuantity = -1);

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
	int32 FindFirstEmptySlot(EInventorySlotType InSlotType = EInventorySlotType::General) const;

	// Count how many of an item we have
	UFUNCTION(BlueprintCallable, Category = "Inventory", BlueprintPure)
	int32 CountItem(const UItemDefinitionAsset* InItemDataAsset) const;

	// Check if we have enough of an item
	UFUNCTION(BlueprintCallable, Category = "Inventory", BlueprintPure)
	bool HasItem(const UItemDefinitionAsset* InItemDataAsset, int32 InQuantity = 1) const;

	// Get all slots
	const FInventorySlotList& GetSlotList() const { return SlotList; }

	// Set slot type
	UFUNCTION(BlueprintCallable, Category = "Inventory", BlueprintAuthorityOnly)
	void SetSlotType(int32 InSlotIndex, EInventorySlotType InSlotType);

	// Set slot restriction tags
	UFUNCTION(BlueprintCallable, Category = "Inventory", BlueprintAuthorityOnly)
	void SetSlotRestrictionTags(int32 InSlotIndex, const FGameplayTagContainer& InRestrictionTags);

	// Lock/unlock a slot
	UFUNCTION(BlueprintCallable, Category = "Inventory", BlueprintAuthorityOnly)
	void SetSlotLocked(int32 InSlotIndex, bool bInLocked);

	// Get fragment dynamic data from item in slot
	template <typename T>
	T* GetSlotItemFragmentDynamicData(int32 InSlotIndex, TSubclassOf<UItemFragment_Base> InFragmentClass)
	{
		FInventorySlot* slot = SlotList.GetSlotMutable(InSlotIndex);
		if (slot == nullptr || slot->IsEmpty())
		{
			return nullptr;
		}

		return slot->ItemInstance.GetFragmentDynamicData<T>(InFragmentClass);
	}

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



};