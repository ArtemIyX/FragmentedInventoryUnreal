// Fill out your copyright notice in the Description page of Project Settings.


#include "Objects/ItemFragment_Base.h"

#include "Data/InventoryItemInstance.h"

UItemFragment_Base::UItemFragment_Base(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	FragmentDisplayName = FText::FromString(TEXT("Base Fragment"));
}

FString UItemFragment_Base::GetDebugString(const FInstancedStruct& InDynamicData) const
{
	return FragmentDisplayName.ToString();
}

void UItemFragment_Base::InitializeDynamicData(FInstancedStruct& OutDynamicData) const
{
	const UScriptStruct* structType = GetDynamicDataStructType();
	if (!IsValid(structType))
	{
		UE_LOG(LogTemp, Error, TEXT("%hs:%d - GetDynamicDataStructType returned null for fragment %s"), __FUNCTION__,
		       __LINE__, *GetNameSafe(this));
		return;
	}

	OutDynamicData.InitializeAs(structType);
}

void UItemFragment_Base::OnItemCreated(FInventoryItemInstance* ItemInstance,
                                       const FInstancedStruct& InDynamicData) const
{
	if (ItemInstance)
		GrantTags(ItemInstance->ItemTags);
}

void UItemFragment_Base::OnItemDestroyed(FInventoryItemInstance* ItemInstance,
                                         const FInstancedStruct& InDynamicData) const
{
	if (ItemInstance)
		RemoveTags(ItemInstance->ItemTags);
}

void UItemFragment_Base::GrantTags(FGameplayTagContainer& ItemTags) const
{
	for (int32 i = 0; i < GrantItemTags.Num(); ++i)
	{
		ItemTags.AddTag(GrantItemTags.GetByIndex(i));
	}
}

void UItemFragment_Base::RemoveTags(FGameplayTagContainer& ItemTags) const
{
	ItemTags.RemoveTags(GrantItemTags);
}
