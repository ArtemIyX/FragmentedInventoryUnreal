// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/Object.h"
#include "ItemFragment_Base.generated.h"

/**
 * 
 
 */
UCLASS(Blueprintable, BlueprintType, Abstract, DefaultToInstanced, EditInlineNew, DisplayName="Item Fragment (Abstract)")
class FRAGMENTEDINVENTORY_API UItemFragment_Base : public UObject
{
	GENERATED_BODY()

public:
	UItemFragment_Base(const FObjectInitializer& ObjectInitializer = FObjectInitializer());

public:
	// Returns the UScriptStruct type for this fragment's dynamic data
	virtual const UScriptStruct* GetDynamicDataStructType() const PURE_VIRTUAL(UItemFragment_Base::GetDynamicDataStructType, return nullptr;);

	// Initialize dynamic data with default values from this fragment's CDO
	virtual void InitializeDynamicData(FInstancedStruct& OutDynamicData) const;

	// Called when item with this fragment is created
	virtual void OnItemCreated(const FInstancedStruct& InDynamicData) const {}

	// Called when item with this fragment is destroyed
	virtual void OnItemDestroyed(const FInstancedStruct& InDynamicData) const {}

public:
	// Display name for editor/debugging
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fragment")
	FText FragmentDisplayName;
};
