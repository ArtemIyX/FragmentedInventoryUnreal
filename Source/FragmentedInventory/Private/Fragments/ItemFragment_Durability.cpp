// Fill out your copyright notice in the Description page of Project Settings.


#include "Fragments/ItemFragment_Durability.h"

UItemFragment_Durability::UItemFragment_Durability(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
	, MaxDurability(100.0f)
	, bDestroyOnBroken(false)
	, bCanBeRepaired(true)

{
	FragmentDisplayName = FText::FromString(TEXT("Durability"));
}

const UScriptStruct* UItemFragment_Durability::GetDynamicDataStructType() const
{
	return FDurabilityDynamicData::StaticStruct();
}

void UItemFragment_Durability::InitializeDynamicData(FInstancedStruct& OutDynamicData) const
{
	Super::InitializeDynamicData(OutDynamicData);
	// Initialize with max durability
	if (OutDynamicData.GetScriptStruct() == FDurabilityDynamicData::StaticStruct())
	{
		FDurabilityDynamicData* dynamicData = OutDynamicData.GetMutablePtr<FDurabilityDynamicData>();
		if (dynamicData != nullptr)
		{
			dynamicData->CurrentDurability = MaxDurability;
			dynamicData->bIsBroken = false;
		}
	}
}
