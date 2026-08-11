// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/InventoryItemInstance.h"

#include "FragmentedInventory.h"
#include "Logging/StructuredLog.h"

FInventoryItemInstance::FInventoryItemInstance()
	: ItemInstanceID(FGuid())
	  , ItemDataAsset(nullptr)
	  , CachedItemDataAsset(nullptr)
{
}

void FInventoryItemInstance::InitializeFromDataAsset(const UItemDefinitionAsset* InItemDataAsset)
{
	if (!IsValid(InItemDataAsset))
	{
		UE_LOGFMT(LogFragmentedInventory, Error, "Invalid item definition passed to InitializeFromDataAsset");
		return;
	}

	Reset();

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
			UE_LOGFMT(LogFragmentedInventory, Warning, "Null fragment in item definition {ItemDefinition}", InItemDataAsset->GetName());
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
	if (const UItemDefinitionAsset* LoadedItemDataAsset = GetItemDataAsset())
	{
		return LoadedItemDataAsset->GetFragmentIndex(InFragmentClass);
	}

	return INDEX_NONE;
}

UItemFragment_Base* FInventoryItemInstance::GetFragmentByClass(TSubclassOf<UItemFragment_Base> InFragmentClass) const
{
	if (const UItemDefinitionAsset* LoadedItemDataAsset = GetItemDataAsset())
	{
		return LoadedItemDataAsset->GetFragmentByClass(InFragmentClass);
	}

	return nullptr;
}


const UItemDefinitionAsset* FInventoryItemInstance::GetItemDataAsset() const
{
	if (IsValid(CachedItemDataAsset.Get()))
	{
		return CachedItemDataAsset.Get();
	}

	if (UItemDefinitionAsset* LoadedItemDataAsset = ItemDataAsset.Get())
	{
		CachedItemDataAsset = LoadedItemDataAsset;
		return LoadedItemDataAsset;
	}

	return nullptr;
}

bool FInventoryItemInstance::IsItemDataAsset(const UItemDefinitionAsset* InItemDataAsset) const
{
	return IsValid(InItemDataAsset) && ItemDataAsset.ToSoftObjectPath() == FSoftObjectPath(InItemDataAsset->GetPathName());
}

void FInventoryItemInstance::Reset()
{
	if (const UItemDefinitionAsset* LoadedItemDataAsset = GetItemDataAsset())
	{
		const TArray<TObjectPtr<UItemFragment_Base>>& Fragments = LoadedItemDataAsset->GetFragments();
		for (const TObjectPtr<UItemFragment_Base>& Fragment : Fragments)
		{
			if (!IsValid(Fragment))
			{
				UE_LOGFMT(LogFragmentedInventory, Warning, "Null fragment in item definition {ItemDefinition}", LoadedItemDataAsset->GetName());
				continue;
			}

			const int32 FragmentIndex = LoadedItemDataAsset->GetFragmentIndex(Fragment->GetClass());
			if (DynamicFragmentData.IsValidIndex(FragmentIndex))
			{
				Fragment->OnItemDestroyed(this, DynamicFragmentData[FragmentIndex]);
			}
		}
	}

	ItemInstanceID = FGuid();
	CachedItemDataAsset = nullptr;
	ItemDataAsset = nullptr;
	DynamicFragmentData.Reset();
	ItemTags = FGameplayTagContainer();
}
