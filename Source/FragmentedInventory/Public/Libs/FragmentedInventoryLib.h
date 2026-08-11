// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/InventoryItemInstance.h"
#include "Data/InventorySlot.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "FragmentedInventoryLib.generated.h"

/**
 *
 */
UCLASS()
class FRAGMENTEDINVENTORY_API UFragmentedInventoryLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Inventory|Slot")
	static bool IsEmpty(const FInventorySlot& InSlot);

	/** @brief Checks whether an empty slot accepts a definition and quantity. Use CanAcceptItemInstance for occupied stacks. */
	UFUNCTION(BlueprintCallable, Category="Inventory|Slot", meta = (DeprecatedFunction, DeprecationMessage = "Use CanAcceptItemInstance for exact occupied-stack validation.", ToolTip = "Checks whether an empty slot accepts a definition and quantity. Use Can Accept Item Instance for occupied stacks."))
	static bool CanAcceptItem(const FInventorySlot& InSlot, const UItemDefinitionAsset* InItemDataAsset,
	                          int32 InQuantity = 1);

	/** @brief Checks whether a slot accepts a specific instance, including conditional stack policy. */
	UFUNCTION(BlueprintCallable, Category="Inventory|Slot", meta = (ToolTip = "Checks whether a slot accepts a specific instance, including conditional stack policy."))
	static bool CanAcceptItemInstance(const FInventorySlot& InSlot, const FInventoryItemInstance& InItemInstance,
		int32 InQuantity = 1);

	UFUNCTION(BlueprintCallable, Category="Inventory|Slot")
	static bool CanStackWith(const FInventorySlot& InSlot, const FInventoryItemInstance& InOtherItem);

	UFUNCTION(BlueprintCallable, Category="Inventory|Slot")
	static int32 GetRemainingStackSpace(const FInventorySlot& InSlot);

	UFUNCTION(BlueprintCallable, Category="Inventory|Slot")
	static int32 GetMaxStackSize(const FInventorySlot& InSlot);

	UFUNCTION(BlueprintCallable, Category="Inventory|Item")
	static bool GetFragmentDynamicData(const FInventoryItemInstance& InItem, TSubclassOf<UItemFragment_Base> InFragmentClass,
		FInstancedStruct& OutDynamicData);

	UFUNCTION(BlueprintCallable, Category="Inventory|Item")
	static bool GetFragmentIndex(const FInventoryItemInstance& InItem, TSubclassOf<UItemFragment_Base> InFragmentClass,
		int32& OutIndex);

	UFUNCTION(BlueprintCallable, Category="Inventory|Item")
	static bool IsValidData(const FInventoryItemInstance& InItem);

	UFUNCTION(BlueprintCallable, Category="Inventory|Item")
	static const UItemDefinitionAsset* GetItemDataAsset(const FInventoryItemInstance& InItem);
};
