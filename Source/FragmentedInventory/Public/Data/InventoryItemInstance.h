// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "GameplayTagContainer.h"
#include "ItemDefinitionAsset.h"

#include "StructUtils/InstancedStruct.h"

#include "InventoryItemInstance.generated.h"
class UItemFragment_Base;

USTRUCT(BlueprintType)
struct FRAGMENTEDINVENTORY_API FInventoryItemInstance
{
	GENERATED_BODY()

public:
	FInventoryItemInstance();

public:
	// Initialize item from data asset
	void InitializeFromDataAsset(const UItemDefinitionAsset* InItemDataAsset);

	// Get dynamic data for a specific fragment type
	template <typename T>
	T* GetFragmentDynamicData(TSubclassOf<UItemFragment_Base> InFragmentClass)
	{
		const int32 fragmentIndex = GetFragmentIndex(InFragmentClass);
		if (fragmentIndex == INDEX_NONE || !DynamicFragmentData.IsValidIndex(fragmentIndex))
		{
			return nullptr;
		}

		return DynamicFragmentData[fragmentIndex].GetMutablePtr<T>();
	}

	// Get const dynamic data for a specific fragment type
	template <typename T>
	const T* GetFragmentDynamicData(TSubclassOf<UItemFragment_Base> InFragmentClass) const
	{
		const int32 fragmentIndex = GetFragmentIndex(InFragmentClass);
		if (fragmentIndex == INDEX_NONE || !DynamicFragmentData.IsValidIndex(fragmentIndex))
		{
			return nullptr;
		}

		return DynamicFragmentData[fragmentIndex].GetPtr<T>();
	}

	// Get fragment index from data asset
	int32 GetFragmentIndex(TSubclassOf<UItemFragment_Base> InFragmentClass) const;

	// Get the item data asset
	const UItemDefinitionAsset* GetItemDataAsset() const { return ItemDataAsset.Get(); }

	// Check if item is valid
	bool IsValidData() const { return ItemDataAsset.IsValid() && ItemInstanceID.IsValid(); }

public:
	// Unique identifier for this specific item instance
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FGuid ItemInstanceID;

	// Reference to the item data asset (the "blueprint" for this item)
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	TSoftObjectPtr<UItemDefinitionAsset> ItemDataAsset;

	// Array of dynamic data for each fragment
	// Indices match the fragment array in ItemDataAsset
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	TArray<FInstancedStruct> DynamicFragmentData;

	// Optional custom tags for gameplay queries
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FGameplayTagContainer ItemTags;

	// Cached pointer to loaded data asset (for fast access, not replicated)
	UPROPERTY(Transient, NotReplicated)
	mutable TObjectPtr<const UItemDefinitionAsset> CachedItemDataAsset;
};
