// Fill out your copyright notice in the Description page of Project Settings.

#include "Fragments/ItemFragment_Stackable.h"

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
