// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/Object.h"
#include "ItemFragment_Base.generated.h"

struct FInventoryItemInstance;
/**
 * 
 
 */
UCLASS(Blueprintable, BlueprintType, Abstract, DefaultToInstanced, EditInlineNew,
	DisplayName="Item Fragment (Abstract)")
class FRAGMENTEDINVENTORY_API UItemFragment_Base : public UObject
{
	GENERATED_BODY()

public:
	UItemFragment_Base(const FObjectInitializer& ObjectInitializer = FObjectInitializer());

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fragment")
	FGameplayTagContainer GrantItemTags;

protected:
	virtual void GrantTags(FGameplayTagContainer& ItemTags) const;
	virtual void RemoveTags(FGameplayTagContainer& ItemTags) const;

public:
	// Returns the UScriptStruct type for this fragment's dynamic data
	virtual const UScriptStruct* GetDynamicDataStructType() const PURE_VIRTUAL(
		UItemFragment_Base::GetDynamicDataStructType, return nullptr;)

	virtual FString GetDebugString(const FInstancedStruct& InDynamicData) const;

	// Initialize dynamic data with default values from this fragment's CDO
	virtual void InitializeDynamicData(FInstancedStruct& OutDynamicData) const;

	// Called when item with this fragment is created
	virtual void OnItemCreated(FInventoryItemInstance* ItemInstance, const FInstancedStruct& InDynamicData) const;

	// Called when item with this fragment is destroyed
	virtual void OnItemDestroyed(FInventoryItemInstance* ItemInstance, const FInstancedStruct& InDynamicData) const;

public:
	// Display name for editor/debugging
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fragment")
	FText FragmentDisplayName;
};