// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/InventoryItemList.h"

#include "Components/FragmentedInventoryComponent.h"

// Fast Array callbacks implementation
void FInventoryItemList::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
	for (int32 Index : RemovedIndices)
	{
		if (IsValid(OwnerComponent))
		{
			OwnerComponent->OnItemRemoved.Broadcast(Items[Index].ItemInstance, Index);
		}
	}
}

void FInventoryItemList::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	for (int32 Index : AddedIndices)
	{
		if (IsValid(OwnerComponent))
		{
			OwnerComponent->OnItemAdded.Broadcast(Items[Index].ItemInstance, Index);
		}
	}
}

void FInventoryItemList::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	for (int32 Index : ChangedIndices)
	{
		if (IsValid(OwnerComponent))
		{
			OwnerComponent->OnItemChanged.Broadcast(Items[Index].ItemInstance, Index);
		}
	}
}