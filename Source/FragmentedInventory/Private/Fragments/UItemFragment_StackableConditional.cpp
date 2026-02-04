// Fill out your copyright notice in the Description page of Project Settings.

#include "Data/InventoryItemInstance.h"
#include "Fragments/ItemFragment_StackableConditional.h"

UItemFragment_StackableConditional::UItemFragment_StackableConditional(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FragmentDisplayName = FText::FromString(TEXT("Stackable Conditional"));
	
	// Conditional stacking doesn't use the legacy flag
	bAllowStackingWithDifferentData = false;
}

FString UItemFragment_StackableConditional::GetStackKey(const FInventoryItemInstance& InItem) const
{
	// Call blueprint implementation if available
	return BP_GetStackKey(InItem);
}

FString UItemFragment_StackableConditional::BP_GetStackKey_Implementation(const FInventoryItemInstance& InItem) const
{
	// Default implementation: each instance is unique (no stacking)
	// Override in C++ or Blueprint to implement custom logic
	return InItem.ItemInstanceID.ToString();
}