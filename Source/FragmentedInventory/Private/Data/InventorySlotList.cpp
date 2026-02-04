// Fill out your copyright notice in the Description page of Project Settings.

#include "Data/InventorySlotList.h"
#include "Components/FragmentedInventoryComponent.h"

void FInventorySlotList::InitializeSlots(int32 InSlotCount, EInventorySlotType InDefaultSlotType)
{
	Slots.Empty(InSlotCount);
	
	for (int32 slotIndex = 0; slotIndex < InSlotCount; ++slotIndex)
	{
		FInventorySlot& newSlot = Slots.AddDefaulted_GetRef();
		newSlot.SlotIndex = slotIndex;
		newSlot.SlotType = InDefaultSlotType;
		newSlot.CurrentStackSize = 0;
		newSlot.bIsLocked = false;
	}
}

const FInventorySlot* FInventorySlotList::GetSlot(int32 InSlotIndex) const
{
	if (!Slots.IsValidIndex(InSlotIndex))
	{
		return nullptr;
	}

	return &Slots[InSlotIndex];
}

FInventorySlot* FInventorySlotList::GetSlotMutable(int32 InSlotIndex)
{
	if (!Slots.IsValidIndex(InSlotIndex))
	{
		return nullptr;
	}

	return &Slots[InSlotIndex];
}

int32 FInventorySlotList::FindFirstEmptySlot(EInventorySlotType InSlotType) const
{
	for (int32 slotIndex = 0; slotIndex < Slots.Num(); ++slotIndex)
	{
		const FInventorySlot& slot = Slots[slotIndex];
		
		// Check if slot matches type and is empty
		if (slot.SlotType == InSlotType && slot.IsEmpty() && !slot.bIsLocked)
		{
			return slotIndex;
		}
	}

	return INDEX_NONE;
}

int32 FInventorySlotList::FindSlotForItem(const UItemDefinitionAsset* InItemDataAsset, int32 InQuantity) const
{
	if (!IsValid(InItemDataAsset))
	{
		return INDEX_NONE;
	}

	// First, try to find a slot with the same item that can stack
	for (int32 slotIndex = 0; slotIndex < Slots.Num(); ++slotIndex)
	{
		const FInventorySlot& slot = Slots[slotIndex];
		
		if (!slot.IsEmpty() && slot.CanAcceptItem(InItemDataAsset, InQuantity))
		{
			return slotIndex;
		}
	}

	// If no stackable slot found, find first empty slot
	for (int32 slotIndex = 0; slotIndex < Slots.Num(); ++slotIndex)
	{
		const FInventorySlot& slot = Slots[slotIndex];
		
		if (slot.IsEmpty() && slot.CanAcceptItem(InItemDataAsset, InQuantity))
		{
			return slotIndex;
		}
	}

	return INDEX_NONE;
}

void FInventorySlotList::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
	// Called before slots are removed from the array
	for (int32 removedIndex : RemovedIndices)
	{
		if (Slots.IsValidIndex(removedIndex))
		{
			const FInventorySlot& slot = Slots[removedIndex];
			
			if (IsValid(OwnerComponent))
			{
				// Notify component about slot being removed/cleared
				// Note: This may not be needed for slot-based inventory since slots don't get removed
			}
		}
	}
}

void FInventorySlotList::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	// Called after new slots are added to the array
	for (int32 addedIndex : AddedIndices)
	{
		if (Slots.IsValidIndex(addedIndex))
		{
			// Slots were added - notify component
			if (IsValid(OwnerComponent))
			{
				// You can add custom logic here if needed
			}
		}
	}
}

void FInventorySlotList::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	// Called after slots are modified
	for (int32 changedIndex : ChangedIndices)
	{
		if (Slots.IsValidIndex(changedIndex))
		{
			const FInventorySlot& slot = Slots[changedIndex];
			
			if (IsValid(OwnerComponent))
			{
				// Broadcast that this slot changed
				OwnerComponent->OnSlotChanged.Broadcast(slot.SlotIndex, slot);
			}
		}
	}
}