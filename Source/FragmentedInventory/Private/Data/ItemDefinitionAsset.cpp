// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/ItemDefinitionAsset.h"

#include "FragmentedInventory.h"
#include "Fragments/ItemFragment_Stackable.h"
#include "Logging/StructuredLog.h"
#include "Misc/DataValidation.h"
#include "Objects/ItemFragment_Base.h"

UItemDefinitionAsset::UItemDefinitionAsset()
{
	AssetType = UItemDefinitionAsset::ItemAssetManagerType;
	ItemDisplayName = FText::FromString(TEXT("New Item"));
	ItemDescription = FText::GetEmpty();
	ItemIcon = nullptr;
}

#if WITH_EDITOR
EDataValidationResult UItemDefinitionAsset::IsDataValid(class FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// Check for null fragments
	for (int32 i = 0; i < Fragments.Num(); ++i)
	{
		if (!IsValid(Fragments[i]))
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("Fragment at index %d is null"), i)));
			Result = EDataValidationResult::Invalid;
		}
	}

	// Check for duplicate fragment types
	TSet<UClass*> FragmentClasses;
	for (const UItemFragment_Base* fragment : Fragments)
	{
		if (IsValid(fragment))
		{
			if (FragmentClasses.Contains(fragment->GetClass()))
			{
				Context.AddError(FText::FromString(
					FString::Printf(TEXT("Duplicate fragment type: %s"), *fragment->GetClass()->GetName())));
				Result = EDataValidationResult::Invalid;
			}
			FragmentClasses.Add(fragment->GetClass());
		}
	}

	if (GetMaxStackSize() <= 0)
	{
		Context.AddError(FText::FromString(TEXT("Max stack size must be greater than zero")));
		Result = EDataValidationResult::Invalid;
	}

	// Validate ItemTypeID is set
	if (AssetId.IsNone())
	{
		Context.AddWarning(FText::FromString(TEXT("Item ID is not set")));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}


#endif
int32 UItemDefinitionAsset::GetMaxStackSize() const
{
	// Query the Stackable fragment if it exists
	const UItemFragment_Stackable* stackableFragment = GetFragment<UItemFragment_Stackable>();

	if (stackableFragment != nullptr)
	{
		// Return the max stack size defined by the Stackable fragment
		return stackableFragment->MaxStackSize;
	}

	// Default: no stacking (single item only)
	return 1;
}

bool UItemDefinitionAsset::IsStackable() const
{
	// Item is stackable if it has a Stackable fragment with MaxStackSize > 1
	const UItemFragment_Stackable* stackableFragment = GetFragment<UItemFragment_Stackable>();
	return stackableFragment != nullptr && stackableFragment->MaxStackSize > 1;
}

UItemFragment_Base* UItemDefinitionAsset::GetFragmentByClass(TSubclassOf<UItemFragment_Base> InFragmentClass) const
{
	if (!InFragmentClass)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "Invalid fragment class passed to GetFragmentByClass");
		return nullptr;
	}

	for (UItemFragment_Base* fragment : Fragments)
	{
		if (IsValid(fragment) && fragment->IsA(InFragmentClass))
		{
			return fragment;
		}
	}

	return nullptr;
}

bool UItemDefinitionAsset::HasFragment(TSubclassOf<UItemFragment_Base> InFragmentClass) const
{
	return GetFragmentByClass(InFragmentClass) != nullptr;
}

int32 UItemDefinitionAsset::GetFragmentIndex(TSubclassOf<UItemFragment_Base> InFragmentClass) const
{
	if (!InFragmentClass)
	{
		return INDEX_NONE;
	}

	for (int32 i = 0; i < Fragments.Num(); ++i)
	{
		if (IsValid(Fragments[i]) && Fragments[i]->IsA(InFragmentClass))
		{
			return i;
		}
	}

	return INDEX_NONE;
}

FGameplayTagContainer UItemDefinitionAsset::GetItemTags() const
{
	FGameplayTagContainer Result = ItemTags;
	for (const UItemFragment_Base* Fragment : Fragments)
	{
		if (IsValid(Fragment))
		{
			Fragment->AppendItemTags(Result);
		}
	}

	return Result;
}

FPrimaryAssetId UItemDefinitionAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(PrimaryAssetType, AssetId.IsNone() ? GetFName() : AssetId);
}
