// Fill out your copyright notice in the Description page of Project Settings.

#include "Data/InventorySlot.h"
#include "Data/ItemDefinitionAsset.h"

bool FInventorySlot::CanAcceptItem(const UItemDefinitionAsset* InItemDataAsset, int32 InQuantity) const
{
	// Check if slot is locked
	if (bIsLocked)
	{
		return false;
	}

	// Check if valid item data asset
	if (!IsValid(InItemDataAsset))
	{
		return false;
	}

	// Check quantity
	if (InQuantity <= 0)
	{
		return false;
	}

	// If slot is empty, check only restriction tags
	if (IsEmpty())
	{
		// If no restriction tags, accept any item
		if (SlotRestrictionTags.IsEmpty())
		{
			return true;
		}

		// Check if item has any of the required tags
		// TODO: This assumes items have tags - you may need to add ItemTags to ItemDefinitionAsset
		// For now, we'll accept if no restrictions
		return true;
	}

	// If slot has an item, check if we can stack
	if (!ItemInstance.IsValidData())
	{
		return false;
	}

	// Check if same item type
	if (ItemInstance.GetItemDataAsset() != InItemDataAsset)
	{
		return false;
	}

	// Check if we have space for the quantity
	const int32 remainingSpace = GetRemainingStackSpace();
	return remainingSpace >= InQuantity;
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
	return GetRemainingStackSpace() > 0;
}