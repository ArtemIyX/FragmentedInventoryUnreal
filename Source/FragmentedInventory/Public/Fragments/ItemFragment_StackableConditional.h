// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Fragments/ItemFragment_Stackable.h"
#include "ItemFragment_StackableConditional.generated.h"

/**
 * Conditional stackable fragment with custom stacking logic
 * Override GetStackKey() to define when items can stack
 * Examples:
 * - Oxygen tanks: stack only if both empty or both full
 * - Water bottles: stack only if both empty
 * - Weapons: stack based on durability ranges
 */
UCLASS(Blueprintable, BlueprintType, DisplayName="Item Fragment - Stackable Conditional")
class FRAGMENTEDINVENTORY_API UItemFragment_StackableConditional : public UItemFragment_Stackable
{
	GENERATED_BODY()

public:
	UItemFragment_StackableConditional(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

public:
	// Override to define custom stack key logic
	// Items with matching keys can stack together
	virtual FString GetStackKey(const FInventoryItemInstance& InItem) const override;

	virtual FString GetDebugString(const FInstancedStruct& InDynamicData) const override;
protected:
	// Blueprint implementable event for custom stack key generation
	// Return a string that represents the "stackability group" of this item
	// Items with the same key can stack together
	UFUNCTION(BlueprintNativeEvent, Category = "Stackable")
	FString BP_GetStackKey(const FInventoryItemInstance& InItem) const;

};