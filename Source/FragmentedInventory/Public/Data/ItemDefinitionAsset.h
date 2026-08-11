// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/AdvancedDataAsset.h"
#include "Engine/AssetManagerTypes.h"
#include "Objects/ItemFragment_Base.h"
#include "ItemDefinitionAsset.generated.h"

class UTexture2D;

/**
 *
 */
UCLASS(Blueprintable, BlueprintType)
class FRAGMENTEDINVENTORY_API UItemDefinitionAsset : public UAdvancedDataAsset
{
	GENERATED_BODY()

public:
	UItemDefinitionAsset();

public:
	/** @brief Fragment instances defining static item behavior. Duplicate fragment classes are invalid. */
	UPROPERTY(EditDefaultsOnly, Instanced, BlueprintReadOnly, Category = "Item Configuration", meta = (ToolTip = "Fragment instances defining static item behavior. Duplicate fragment classes are invalid."))
	TArray<TObjectPtr<UItemFragment_Base>> Fragments;

	/** @brief Tags used by slot restrictions before an item instance exists. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Configuration", meta = (ToolTip = "Tags used by slot restrictions before an item instance exists."))
	FGameplayTagContainer ItemTags;

	/** @brief Human-readable item name shown by inventory UI. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Display", meta = (ToolTip = "Human-readable item name shown by inventory UI."))
	FText ItemDisplayName;

	/** @brief Description shown in item tooltips and UI. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Display", meta = (MultiLine = true, ToolTip = "Description shown in item tooltips and UI."))
	FText ItemDescription;

	/** @brief Optional inventory icon loaded on demand by the UI. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Display", meta = (ToolTip = "Optional inventory icon loaded on demand by the UI."))
	TSoftObjectPtr<UTexture2D> ItemIcon;
public:
#if WITH_EDITOR
	// Validate fragment configuration in editor
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

public:

	/** @brief Gets the maximum stack size. Returns one when no stackable fragment exists. */
	UFUNCTION(BlueprintCallable, Category = "Item")
	int32 GetMaxStackSize() const;

	// Check if this item is stackable (has Stackable fragment)
	UFUNCTION(BlueprintCallable, Category = "Item")
	bool IsStackable() const;

	// Get fragment instance by class type
	template<typename T>
	T* GetFragment() const
	{
		static_assert(TIsDerivedFrom<T, UItemFragment_Base>::Value, "T must derive from UItemFragment_Base");

		for (UItemFragment_Base* fragment : Fragments)
		{
			if (IsValid(fragment) && fragment->IsA<T>())
			{
				return Cast<T>(fragment);
			}
		}

		return nullptr;
	}


	// Get fragment instance by class
	UItemFragment_Base* GetFragmentByClass(TSubclassOf<UItemFragment_Base> InFragmentClass) const;

	// Check if this item has a specific fragment type
	bool HasFragment(TSubclassOf<UItemFragment_Base> InFragmentClass) const;

	// Get index of fragment in array (useful for dynamic data array indexing)
	int32 GetFragmentIndex(TSubclassOf<UItemFragment_Base> InFragmentClass) const;

	/** @brief Gets static tags supplied by the item definition and its fragments. */
	FGameplayTagContainer GetItemTags() const;

	/** @brief Gets this item's Asset Manager identifier. */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// Get all fragments
	const TArray<TObjectPtr<UItemFragment_Base>>& GetFragments() const { return Fragments; }
public:
	inline static const FName ItemAssetManagerType = FName(TEXT("FragmentedInventoryItem"));

	inline static const FPrimaryAssetType PrimaryAssetType = FPrimaryAssetType(ItemAssetManagerType);
};
