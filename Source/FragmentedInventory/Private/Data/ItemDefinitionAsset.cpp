// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/ItemDefinitionAsset.h"

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

	// Validate ItemTypeID is set
	if (AssetId.IsNone())
	{
		Context.AddWarning(FText::FromString(TEXT("Item ID is not set")));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif


UItemFragment_Base* UItemDefinitionAsset::GetFragmentByClass(TSubclassOf<UItemFragment_Base> InFragmentClass) const
{
	if (!InFragmentClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("%hs:%d - Invalid fragment class passed to GetFragmentByClass"), __FUNCTION__,
		       __LINE__);
		return nullptr;
	}

	for (UItemFragment_Base* fragment : Fragments)
	{
		if (IsValid(fragment) && fragment->GetClass() == InFragmentClass)
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
		if (IsValid(Fragments[i]) && Fragments[i]->GetClass() == InFragmentClass)
		{
			return i;
		}
	}

	return INDEX_NONE;
}
