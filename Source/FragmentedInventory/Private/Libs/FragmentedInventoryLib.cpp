// Fill out your copyright notice in the Description page of Project Settings.


#include "Libs/FragmentedInventoryLib.h"

#include "Data/InventorySlot.h"

bool UFragmentedInventoryLib::IsEmpty(const FInventorySlot& InSlot)
{
	return InSlot.IsEmpty();
}

bool UFragmentedInventoryLib::CanAcceptItem(const FInventorySlot& InSlot, const UItemDefinitionAsset* InItemDataAsset,
                                            int32 InQuantity)
{
	return InSlot.CanAcceptItem(InItemDataAsset, InQuantity);
}

bool UFragmentedInventoryLib::CanStackWith(const FInventorySlot& InSlot, const FInventoryItemInstance& InOtherItem)
{
	return InSlot.CanStackWith(InOtherItem);
}

int32 UFragmentedInventoryLib::GetRemainingStackSpace(const FInventorySlot& InSlot)
{
	return InSlot.GetRemainingStackSpace();
}

int32 UFragmentedInventoryLib::GetMaxStackSize(const FInventorySlot& InSlot)
{
	return InSlot.GetMaxStackSize();
}

bool UFragmentedInventoryLib::GetFragmentDynamicData(const FInventoryItemInstance& InItem,
                                                     TSubclassOf<UItemFragment_Base> InFragmentClass,
                                                     FInstancedStruct& OutDynamicData)
{
	OutDynamicData.Reset();
	const int32 fragmentIndex = InItem.GetFragmentIndex(InFragmentClass);
	if (fragmentIndex == INDEX_NONE || !InItem.DynamicFragmentData.IsValidIndex(fragmentIndex))
	{
		return false;
	}
	OutDynamicData = InItem.DynamicFragmentData[fragmentIndex];
	return true;
}

bool UFragmentedInventoryLib::GetFragmentIndex(const FInventoryItemInstance& InItem,
                                               TSubclassOf<UItemFragment_Base> InFragmentClass, int32& OutIndex)
{
	OutIndex = INDEX_NONE;

	const int32 fragmentIndex = InItem.GetFragmentIndex(InFragmentClass);
	OutIndex = fragmentIndex;
	return OutIndex != INDEX_NONE && InItem.DynamicFragmentData.IsValidIndex(fragmentIndex);
}

bool UFragmentedInventoryLib::IsValidData(const FInventoryItemInstance& InItem)
{
	return InItem.IsValidData();
}

const UItemDefinitionAsset* UFragmentedInventoryLib::GetItemDataAsset(const FInventoryItemInstance& InItem)
{
	return InItem.GetItemDataAsset();
}
