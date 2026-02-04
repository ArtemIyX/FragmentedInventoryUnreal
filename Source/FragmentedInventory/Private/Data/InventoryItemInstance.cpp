// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/InventoryItemInstance.h"


FInventoryItemInstance::FInventoryItemInstance()
	: ItemInstanceID(FGuid::NewGuid())
	  , ItemDataAsset(nullptr)
	  , CachedItemDataAsset(nullptr)
{
}

void FInventoryItemInstance::InitializeFromDataAsset(const UItemDefinitionAsset* InItemDataAsset)
{
	if (!IsValid(InItemDataAsset))
	{
		UE_LOG(LogTemp, Error, TEXT("%hs:%d - Invalid ItemDataAsset passed to InitializeFromDataAsset"), __FUNCTION__,
		       __LINE__);
		return;
	}

	// Generate new unique ID
	ItemInstanceID = FGuid::NewGuid();

	// Store reference to data asset
	ItemDataAsset = TSoftObjectPtr<UItemDefinitionAsset>(const_cast<UItemDefinitionAsset*>(InItemDataAsset));

	CachedItemDataAsset = InItemDataAsset;

	// Initialize dynamic data for each fragment
	const TArray<TObjectPtr<UItemFragment_Base>>& Fragments = InItemDataAsset->GetFragments();
	DynamicFragmentData.Reset(Fragments.Num());

	for (const TObjectPtr<UItemFragment_Base>& fragment : Fragments)
	{
		if (!IsValid(fragment))
		{
			UE_LOG(LogTemp, Warning, TEXT("%hs:%d - Null fragment in ItemDataAsset %s"), __FUNCTION__, __LINE__,
			       *InItemDataAsset->GetName());
			DynamicFragmentData.AddDefaulted();
			continue;
		}

		// Let each fragment initialize its own dynamic data
		FInstancedStruct dynamicData;
		fragment->InitializeDynamicData(dynamicData);
		DynamicFragmentData.Add(MoveTemp(dynamicData));

		// Call fragment's OnItemCreated hook
		fragment->OnItemCreated(this, DynamicFragmentData.Last());
	}
}

int32 FInventoryItemInstance::GetFragmentIndex(TSubclassOf<UItemFragment_Base> InFragmentClass) const
{
	// Try cached pointer first
	if (IsValid(CachedItemDataAsset.Get()))
	{
		return CachedItemDataAsset->GetFragmentIndex(InFragmentClass);
	}

	// Load if needed
	if (ItemDataAsset.IsValid())
	{
		const UItemDefinitionAsset* loadedAsset = ItemDataAsset.LoadSynchronous();
		if (IsValid(loadedAsset))
		{
			CachedItemDataAsset = loadedAsset;
			return loadedAsset->GetFragmentIndex(InFragmentClass);
		}
	}

	return INDEX_NONE;
}

void FInventoryItemInstance::Reset()
{
	if (!IsValidData())
		return;

	if (CachedItemDataAsset == nullptr)
		return;

	const TArray<TObjectPtr<UItemFragment_Base>>& fragments = CachedItemDataAsset->GetFragments();
	for (const TObjectPtr<UItemFragment_Base>& fragment : fragments)
	{
		if (!IsValid(fragment))
		{
			UE_LOG(LogTemp, Warning, TEXT("%hs:%d - Null fragment in ItemDataAsset %s"), __FUNCTION__, __LINE__,
			       *CachedItemDataAsset->GetName());
			continue;
		}

		// Call fragment's OnItemCreated hook
		fragment->OnItemDestroyed(this, DynamicFragmentData.Last());
	}

	this->ItemInstanceID = FGuid();
	this->CachedItemDataAsset = nullptr;
	this->ItemDataAsset = nullptr;
	this->DynamicFragmentData = {};
	this->ItemTags = FGameplayTagContainer();
}
