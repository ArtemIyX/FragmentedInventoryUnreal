// Fill out your copyright notice in the Description page of Project Settings.

#include "Fragments/ItemFragment_Stackable.h"

#include "Data/InventoryItemInstance.h"

UItemFragment_Stackable::UItemFragment_Stackable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, MaxStackSize(99)
	, bAllowStackingWithDifferentData(true)
{
	FragmentDisplayName = FText::FromString(TEXT("Stackable"));
}

const UScriptStruct* UItemFragment_Stackable::GetDynamicDataStructType() const
{
	return FStackableDynamicData::StaticStruct();
}

void UItemFragment_Stackable::InitializeDynamicData(FInstancedStruct& OutDynamicData) const
{
	Super::InitializeDynamicData(OutDynamicData);
	
	// Initialize with default stackable data
	if (OutDynamicData.GetScriptStruct() == FStackableDynamicData::StaticStruct())
	{
		FStackableDynamicData* dynamicData = OutDynamicData.GetMutablePtr<FStackableDynamicData>();
		if (dynamicData != nullptr)
		{
			// Initialize any runtime data here if needed
		}
	}
}

FString UItemFragment_Stackable::GetDebugString(const FInstancedStruct& InDynamicData) const
{
	const FString super = Super::GetDebugString(InDynamicData);
	if (!InDynamicData.IsValid())
	{
		return super;
	}
	if (InDynamicData.GetScriptStruct() != FStackableDynamicData::StaticStruct())
	{
		return super;
	}
	const FStackableDynamicData* dynamicData = InDynamicData.GetPtr<FStackableDynamicData>();
	if (dynamicData == nullptr)
	{
		return super;
	}
	return super;
	/*return FString::Printf(TEXT("%s: %d/%d"), *super,
		dynamicData->);*/
}

bool UItemFragment_Stackable::CanStackWith(const FInventoryItemInstance& InItemA, const FInventoryItemInstance& InItemB) const
{
	// Must be same item definition
	if (InItemA.GetItemDataAsset() != InItemB.GetItemDataAsset())
	{
		return false;
	}

	// Use stack key comparison for flexible stacking logic
	const FString keyA = GetStackKey(InItemA);
	const FString keyB = GetStackKey(InItemB);

	return keyA.Equals(keyB);
}

FString UItemFragment_Stackable::GetStackKey(const FInventoryItemInstance& InItem) const
{
	// Default behavior: all instances of this item can stack together
	// Return a constant key for unconditional stacking (like bullets, resources)
	
	if (bAllowStackingWithDifferentData)
	{
		// All instances stack together regardless of dynamic data
		return TEXT("default");
	}
	
	// If not allowing different data, use the instance ID as key
	// This means each instance is unique and won't stack
	return InItem.ItemInstanceID.ToString();
}