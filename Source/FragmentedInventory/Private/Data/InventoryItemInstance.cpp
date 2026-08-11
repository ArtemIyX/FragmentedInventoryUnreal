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

void FInventoryItemInstance::InitializeFromDataAsset(const UItemDefinitionAsset* InItemDataAsset, bool bInInvokeCreatedCallbacks)
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

	}

	if (bInInvokeCreatedCallbacks)
	{
		InvokeCreatedCallbacks();
	}
}

bool FInventoryItemInstance::InitializeFromExistingInstance(const FInventoryItemInstance& InSourceInstance,
	bool bInInvokeCreatedCallbacks)
{
	if (this == &InSourceInstance)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "Cannot clone an inventory item instance into itself");
		return false;
	}

	const UItemDefinitionAsset* SourceItemDataAsset = InSourceInstance.GetItemDataAsset();
	if (!InSourceInstance.IsValidData() || !IsValid(SourceItemDataAsset))
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "Cannot clone an invalid inventory item instance");
		return false;
	}

	Reset();
	ItemInstanceID = FGuid::NewGuid();
	ItemDataAsset = InSourceInstance.ItemDataAsset;
	CachedItemDataAsset = SourceItemDataAsset;
	ItemTags = InSourceInstance.ItemTags;
	DynamicFragmentData = InSourceInstance.DynamicFragmentData;

	if (bInInvokeCreatedCallbacks)
	{
		InvokeCreatedCallbacks();
	}

	return true;
}

void FInventoryItemInstance::InvokeCreatedCallbacks()
{
	if (bLifecycleCallbacksInvoked)
	{
		return;
	}

	const UItemDefinitionAsset* LoadedItemDataAsset = GetItemDataAsset();
	if (!IsValid(LoadedItemDataAsset))
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "Cannot invoke creation callbacks for an invalid inventory item instance");
		return;
	}

	const TArray<TObjectPtr<UItemFragment_Base>>& Fragments = LoadedItemDataAsset->GetFragments();
	for (int32 FragmentIndex = 0; FragmentIndex < Fragments.Num(); ++FragmentIndex)
	{
		const UItemFragment_Base* Fragment = Fragments[FragmentIndex];
		if (!IsValid(Fragment))
		{
			UE_LOGFMT(LogFragmentedInventory, Warning, "Null fragment in item definition {ItemDefinition}", LoadedItemDataAsset->GetName());
			continue;
		}

		if (!DynamicFragmentData.IsValidIndex(FragmentIndex))
		{
			UE_LOGFMT(LogFragmentedInventory, Warning, "Missing dynamic data for fragment {Fragment} while creating item {ItemDefinition}", Fragment->GetName(), LoadedItemDataAsset->GetName());
			continue;
		}

		Fragment->OnItemCreated(this, DynamicFragmentData[FragmentIndex]);
	}

	bLifecycleCallbacksInvoked = true;
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

bool FInventoryItemInstance::IsItemDataAssetLoaded() const
{
	return IsValid(GetItemDataAsset());
}

FSoftObjectPath FInventoryItemInstance::GetItemDataAssetPath() const
{
	return ItemDataAsset.ToSoftObjectPath();
}

void FInventoryItemInstance::Reset()
{
	if (bLifecycleCallbacksInvoked)
	{
		if (const UItemDefinitionAsset* LoadedItemDataAsset = GetItemDataAsset())
		{
			const TArray<TObjectPtr<UItemFragment_Base>>& Fragments = LoadedItemDataAsset->GetFragments();
			for (int32 FragmentIndex = 0; FragmentIndex < Fragments.Num(); ++FragmentIndex)
			{
				const UItemFragment_Base* Fragment = Fragments[FragmentIndex];
				if (!IsValid(Fragment))
				{
					UE_LOGFMT(LogFragmentedInventory, Warning, "Null fragment in item definition {ItemDefinition}", LoadedItemDataAsset->GetName());
					continue;
				}

				if (DynamicFragmentData.IsValidIndex(FragmentIndex))
				{
					Fragment->OnItemDestroyed(this, DynamicFragmentData[FragmentIndex]);
				}
				else
				{
					UE_LOGFMT(LogFragmentedInventory, Warning, "Missing dynamic data for fragment {Fragment} while resetting item {ItemDefinition}", Fragment->GetName(), LoadedItemDataAsset->GetName());
				}
			}
		}
	}

	ItemInstanceID = FGuid();
	bLifecycleCallbacksInvoked = false;
	CachedItemDataAsset = nullptr;
	ItemDataAsset = nullptr;
	DynamicFragmentData.Reset();
	ItemTags = FGameplayTagContainer();
}
