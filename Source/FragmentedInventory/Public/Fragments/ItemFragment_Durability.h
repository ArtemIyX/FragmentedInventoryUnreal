// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Objects/ItemFragment_Base.h"
#include "ItemFragment_Durability.generated.h"

// Dynamic data for durability - this is per-item-instance
USTRUCT(BlueprintType)
struct FRAGMENTEDINVENTORY_API FDurabilityDynamicData
{
	GENERATED_BODY()

public:
	FDurabilityDynamicData()
		: CurrentDurability(0.0f)
		, bIsBroken(false)
	{
	}

	// Current durability value (runtime, per instance)
	UPROPERTY(BlueprintReadWrite, Category = "Durability")
	float CurrentDurability;

	// Whether this item is broken (durability reached 0)
	UPROPERTY(BlueprintReadWrite, Category = "Durability")
	bool bIsBroken;
};

/**
 *
 */
UCLASS(Blueprintable, BlueprintType)
class FRAGMENTEDINVENTORY_API UItemFragment_Durability : public UItemFragment_Base
{
	GENERATED_BODY()

public:
	UItemFragment_Durability(const FObjectInitializer& ObjectInitializer = FObjectInitializer());

public:
	virtual const UScriptStruct* GetDynamicDataStructType() const override;
	virtual void InitializeDynamicData(FInstancedStruct& OutDynamicData) const override;

	virtual FString GetDebugString(const FInstancedStruct& InDynamicData) const override;

public:
	/** @brief Initial and maximum durability. Valid range is one through 10000. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Durability", meta = (ClampMin = "1.0", ClampMax = "10000.0", ToolTip = "Initial and maximum durability. Valid range is one through 10000."))
	float MaxDurability;

	/** @brief Destroys the item when durability reaches zero. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Durability", meta = (ToolTip = "Destroys the item when durability reaches zero."))
	bool bDestroyOnBroken;

	/** @brief Allows repair systems to increase durability after damage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Durability", meta = (ToolTip = "Allows repair systems to increase durability after damage."))
	bool bCanBeRepaired;
};
