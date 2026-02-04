// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InventoryItemInstance.h"
#include "Net/Serialization/FastArraySerializer.h"

#include "InventoryItemEntry.generated.h"

// First, we need the Fast Array Item wrapper
USTRUCT(BlueprintType)
struct FRAGMENTEDINVENTORY_API FInventoryItemEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

public:
	FInventoryItemEntry()
		: ItemInstance()
	{
	}

	FInventoryItemEntry(const FInventoryItemInstance& InItemInstance)
		: ItemInstance(InItemInstance)
	{
	}

	// The actual item data
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FInventoryItemInstance ItemInstance;

	// FFastArraySerializerItem provides ReplicationID and ReplicationKey automatically
};
