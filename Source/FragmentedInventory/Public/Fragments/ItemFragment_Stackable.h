// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Objects/ItemFragment_Base.h"
#include "ItemFragment_Stackable.generated.h"

// Dynamic data for stackable items (currently none needed, but structure in place)
USTRUCT(BlueprintType)
struct FRAGMENTEDINVENTORY_API FStackableDynamicData
{
	GENERATED_BODY()

public:
	// Could add runtime stack-specific data here if needed
	// For example: current stack count per instance (though that's in the slot)
};

/**
 * Fragment that makes an item stackable with a specific max stack size
 * Only items with this fragment can stack beyond 1
 */
UCLASS(Blueprintable, BlueprintType, DisplayName="Item Fragment - Stackable")
class FRAGMENTEDINVENTORY_API UItemFragment_Stackable : public UItemFragment_Base
{
	GENERATED_BODY()

public:
	UItemFragment_Stackable(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

public:
	// Returns the dynamic data struct type for this fragment
	virtual const UScriptStruct* GetDynamicDataStructType() const override;

	// Initialize dynamic data
	virtual void InitializeDynamicData(FInstancedStruct& OutDynamicData) const override;

	virtual FString GetDebugString(const FInstancedStruct& InDynamicData) const override;
public:
	// Maximum number of items that can stack in a single slot
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stackable", meta = (ClampMin = "1", ClampMax = "9999"))
	int32 MaxStackSize;

	// Whether items with different dynamic data can stack together
	// (e.g., can damaged items stack with pristine items?)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stackable")
	bool bAllowStackingWithDifferentData;
};