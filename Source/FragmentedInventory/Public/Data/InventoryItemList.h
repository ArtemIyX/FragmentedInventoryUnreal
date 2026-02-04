// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InventoryItemEntry.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "InventoryItemList.generated.h"


class UFragmentedInventoryComponent;
// Fast Array container
USTRUCT(BlueprintType)
struct FRAGMENTEDINVENTORY_API FInventoryItemList : public FFastArraySerializer
{
	GENERATED_BODY()

public:
	// Array of items - this is what actually replicates
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TArray<FInventoryItemEntry> Items;

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
		return FFastArraySerializer::FastArrayDeltaSerialize<FInventoryItemEntry, FInventoryItemList>(
			Items, DeltaParms, *this);
	}
};

template <>
struct TStructOpsTypeTraits<FInventoryItemList> : public TStructOpsTypeTraitsBase2<FInventoryItemList>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};
