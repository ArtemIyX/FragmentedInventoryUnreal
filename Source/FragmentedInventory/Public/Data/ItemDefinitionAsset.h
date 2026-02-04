// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/AdvancedDataAsset.h"
#include "Objects/ItemFragment_Base.h"
#include "ItemDefinitionAsset.generated.h"

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
	// Array of fragment instances that define this item's capabilities
	// These are CDOs that hold constant data
	UPROPERTY(EditDefaultsOnly, Instanced, BlueprintReadOnly, Category = "Item Configuration")
	TArray<TObjectPtr<UItemFragment_Base>> Fragments;
	
	// Human-readable display name
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Display")
	FText ItemDisplayName;

	// Description for tooltips/UI
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Display", meta = (MultiLine = true))
	FText ItemDescription;

	// Icon for inventory UI
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Display")
	TObjectPtr<UTexture2D> ItemIcon;
public:
#if WITH_EDITOR
	// Validate fragment configuration in editor
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

public:

	// Get maximum stack size for this item
	// Returns 1 if item has no Stackable fragment, otherwise returns the fragment's MaxStackSize
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

	// Get all fragments
	const TArray<TObjectPtr<UItemFragment_Base>>& GetFragments() const { return Fragments; }
public:
	inline static const FName ItemAssetManagerType = FName(TEXT("FragmentedInventoryItem"));
};