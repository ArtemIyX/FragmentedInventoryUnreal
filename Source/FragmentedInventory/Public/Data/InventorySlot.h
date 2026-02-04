// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InventoryItemInstance.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "GameplayTagContainer.h"

#include "InventorySlot.generated.h"

// Enum for slot types
UENUM(BlueprintType)
enum class EInventorySlotType : uint8
{
	General		UMETA(DisplayName = "General"),
	Equipment	UMETA(DisplayName = "Equipment"),
	Hotbar		UMETA(DisplayName = "Hotbar"),
	Ammo		UMETA(DisplayName = "Ammo"),
	Quest		UMETA(DisplayName = "Quest")
};

// Fast Array Item wrapper for a single inventory slot
USTRUCT(BlueprintType)
struct FRAGMENTEDINVENTORY_API FInventorySlot : public FFastArraySerializerItem
{
	GENERATED_BODY()

public:
	FInventorySlot()
		: SlotIndex(INDEX_NONE)
		, SlotType(EInventorySlotType::General)
		, CurrentStackSize(0)
		, bIsLocked(false)
	{
	}

	FInventorySlot(int32 InSlotIndex, EInventorySlotType InSlotType = EInventorySlotType::General)
		: SlotIndex(InSlotIndex)
		, SlotType(InSlotType)
		, CurrentStackSize(0)
		, bIsLocked(false)
	{
	}

	// Check if slot is empty
	bool IsEmpty() const { return !ItemInstance.IsValidData(); }

	// Check if slot can accept an item
	bool CanAcceptItem(const UItemDefinitionAsset* InItemDataAsset, int32 InQuantity = 1) const;

	// Check if this slot can stack with another item
	bool CanStackWith(const FInventoryItemInstance& InOtherItem) const;

	// Get remaining stack space
	int32 GetRemainingStackSpace() const;
	
	// Get maximum stack size for current item in slot (queries item's Stackable fragment)
	int32 GetMaxStackSize() const;

public:
	// The index of this slot in the inventory (fixed)
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	int32 SlotIndex;

	// Type of slot (for restrictions)
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	EInventorySlotType SlotType;

	// Tags that restrict what items can be placed in this slot
	// If empty, any item can be placed. If filled, item must have matching tag
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	FGameplayTagContainer SlotRestrictionTags;

	// The item instance stored in this slot (invalid if empty)
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	FInventoryItemInstance ItemInstance;

	// Current number of items stacked in this slot
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	int32 CurrentStackSize;

	// Whether this slot is locked (cannot be modified)
	UPROPERTY(BlueprintReadOnly, Category = "Slot")
	bool bIsLocked;
};