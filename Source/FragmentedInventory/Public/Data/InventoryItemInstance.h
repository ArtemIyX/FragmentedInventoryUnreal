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
	void InitializeFromDataAsset(const UItemDefinitionAsset* InItemDataAsset, bool bInInvokeCreatedCallbacks = true);

	/** @brief Initializes a new logical instance from an existing instance and invokes creation callbacks for the clone. */
	bool InitializeFromExistingInstance(const FInventoryItemInstance& InSourceInstance);

	template <typename T>
	T* GetFragment() const
	{
		if (const UItemDefinitionAsset* LoadedItemDataAsset = GetItemDataAsset())
		{
			return LoadedItemDataAsset->GetFragment<T>();
		}

		return nullptr;
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
	UItemFragment_Base* GetFragmentByClass(TSubclassOf<UItemFragment_Base> InFragmentClass) const;


	/** @brief Gets the already-loaded item definition without blocking. */
	const UItemDefinitionAsset* GetItemDataAsset() const;

	/** @brief Checks an item definition by soft path without requiring the asset to be loaded. */
	bool IsItemDataAsset(const UItemDefinitionAsset* InItemDataAsset) const;

	/** @brief Returns true when the item definition has finished loading locally. */
	bool IsItemDataAssetLoaded() const;

	/** @brief Gets the replicated item-definition path for asynchronous loading. */
	FSoftObjectPath GetItemDataAssetPath() const;

	// Check if item is valid
	bool IsValidData() const { return ItemInstanceID.IsValid() && !ItemDataAsset.IsNull(); }

	void Reset();

public:
	// Unique identifier for this specific item instance
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FGuid ItemInstanceID;


	// Reference to the item data asset (the "blueprint" for this item)
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	TSoftObjectPtr<UItemDefinitionAsset> ItemDataAsset;

	// Optional custom tags for gameplay queries
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FGameplayTagContainer ItemTags;

	// Cached pointer to loaded data asset (for fast access, not replicated)
	UPROPERTY(Transient, NotReplicated)
	mutable TObjectPtr<const UItemDefinitionAsset> CachedItemDataAsset;

private:
	friend class UFragmentedInventoryComponent;
	friend class UFragmentedInventoryLib;

	/** @brief Per-fragment runtime data. Mutate only through the inventory component. */
	UPROPERTY(BlueprintReadOnly, Category = "Item", meta = (AllowPrivateAccess = "true"))
	TArray<FInstancedStruct> DynamicFragmentData;
};
