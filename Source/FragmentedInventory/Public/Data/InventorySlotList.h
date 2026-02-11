// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InventorySlot.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "InventorySlotList.generated.h"

class UFragmentedInventoryComponent;

// Fast Array container for inventory slots
USTRUCT(BlueprintType)
struct FRAGMENTEDINVENTORY_API FInventorySlotList : public FFastArraySerializer
{
	GENERATED_BODY()

public:
	FInventorySlotList()
		: OwnerComponent(nullptr)
	{
	}

	// Initialize slots with a specific count and type
	void InitializeSlots(int32 InSlotCount, EInventorySlotType InDefaultSlotType = EInventorySlotType::General);

	// Get slot by index (const)
	const FInventorySlot* GetSlot(int32 InSlotIndex) const;

	// Get slot by index (mutable)
	FInventorySlot* GetSlotMutable(int32 InSlotIndex);

	// Get number of slots
	int32 GetSlotCount() const { return Slots.Num(); }

	// Find first empty slot of a specific type
	int32 FindFirstEmptySlot() const;
	int32 FindFirstEmptySlot(EInventorySlotType InSlotTyp) const;

	// Find first slot that can accept the item (empty or stackable)
	int32 FindSlotForItem(const UItemDefinitionAsset* InItemDataAsset, int32 InQuantity = 1) const;

public:
	// Array of slots - this is what actually replicates
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TArray<FInventorySlot> Slots;

	// Owner component reference
	UPROPERTY(NotReplicated)
	TObjectPtr<UFragmentedInventoryComponent> OwnerComponent;

	// Fast array callbacks
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);

	// Required for FFastArraySerializer
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FInventorySlot, FInventorySlotList>(
			Slots, DeltaParms, *this);
	}
};

template <>
struct TStructOpsTypeTraits<FInventorySlotList> : public TStructOpsTypeTraitsBase2<FInventorySlotList>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};
