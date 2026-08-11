// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/InventorySlot.h"
#include "Data/ItemDefinitionAsset.h"
#include "Fragments/ItemFragment_Stackable.h"

int32 FInventorySlot::GetMaxStackSize() const
{
	// If slot is empty, return a default max value (or 1)
	if (IsEmpty())
	{
		return 1;
	}

	// Query the item's definition for its max stack size
	const UItemDefinitionAsset* itemDataAsset = ItemInstance.GetItemDataAsset();
	if (IsValid(itemDataAsset))
	{
		return itemDataAsset->GetMaxStackSize();
	}

	// Fallback to 1 if no valid item
	return 1;
}

int32 FInventorySlot::GetRemainingStackSpace() const
{
	return FMath::Max(0, GetMaxStackSize() - CurrentStackSize);
}

bool FInventorySlot::CanAcceptItem(const UItemDefinitionAsset* InItemDataAsset, int32 InQuantity) const
{
	if (!CanPlaceItem(InItemDataAsset))
	{
		return false;
	}

	// Check quantity
	if (InQuantity <= 0)
	{
		return false;
	}

	if (!IsEmpty())
	{
		return false;
	}

	const int32 MaxStackSize = InItemDataAsset->GetMaxStackSize();
	return MaxStackSize > 0 && InQuantity <= MaxStackSize;
}

bool FInventorySlot::CanAcceptItemInstance(const FInventoryItemInstance& InItemInstance, int32 InQuantity) const
{
	const UItemDefinitionAsset* ItemDataAsset = InItemInstance.GetItemDataAsset();
	if (!InItemInstance.IsValidData() || !IsValid(ItemDataAsset) || InQuantity <= 0)
	{
		return false;
	}

	if (IsEmpty())
	{
		return CanAcceptItem(ItemDataAsset, InQuantity);
	}

	return CanStackWith(InItemInstance) && GetRemainingStackSpace() >= InQuantity;
}

bool FInventorySlot::CanPlaceItem(const UItemDefinitionAsset* InItemDataAsset) const
{
	if (bIsLocked || !IsValid(InItemDataAsset))
	{
		return false;
	}

	return SlotRestrictionTags.IsEmpty() || InItemDataAsset->GetItemTags().HasAny(SlotRestrictionTags);
}

bool FInventorySlot::CanStackWith(const FInventoryItemInstance& InOtherItem) const
{
	// Can't stack if slot is empty
	if (IsEmpty())
	{
		return false;
	}

	// Can't stack if other item is invalid
	if (!InOtherItem.IsValidData())
	{
		return false;
	}

	// Can't stack if locked
	if (bIsLocked)
	{
		return false;
	}

	// Check if same item type
	if (ItemInstance.GetItemDataAsset() != InOtherItem.GetItemDataAsset())
	{
		return false;
	}

	// Check if we have space
	if (GetRemainingStackSpace() <= 0)
	{
		return false;
	}

	// Delegate to the Stackable fragment for stacking logic
	const UItemDefinitionAsset* itemDataAsset = ItemInstance.GetItemDataAsset();
	if (!IsValid(itemDataAsset))
	{
		return false;
	}

	const UItemFragment_Stackable* stackableFragment = itemDataAsset->GetFragment<UItemFragment_Stackable>();
	if (stackableFragment == nullptr)
	{
		// Item is not stackable (no Stackable fragment)
		return false;
	}

	// Let the fragment decide if these instances can stack
	return stackableFragment->CanStackWith(ItemInstance, InOtherItem);
}
