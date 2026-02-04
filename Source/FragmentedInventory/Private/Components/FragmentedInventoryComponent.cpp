// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/FragmentedInventoryComponent.h"

#include "Engine/ActorChannel.h"
#include "Net/UnrealNetwork.h"


UFragmentedInventoryComponent::UFragmentedInventoryComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
	InventoryList.OwnerComponent = this;
}

void UFragmentedInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(UFragmentedInventoryComponent, InventoryList, SharedParams);
}

bool UFragmentedInventoryComponent::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch,
                                                        FReplicationFlags* RepFlags)
{
	bool bWroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

	/*
	for (FInventoryItemEntry& Entry : InventoryList.Items)
	{
		if (Entry.ItemInstance.CachedItemDataAsset)
		{
			bWroteSomething |= Channel->ReplicateSubobject(
				const_cast<UItemDefinitionAsset*>(Entry.ItemInstance.CachedItemDataAsset.Get()), *Bunch, *RepFlags);
		}
	}*/

	return bWroteSomething;
}

bool UFragmentedInventoryComponent::AddItem(const UItemDefinitionAsset* InItemDataAsset, int32& OutItemIndex)
{

	if (!IsValid(InItemDataAsset))
	{
		UE_LOG(LogTemp, Error, TEXT("%hs:%d - Invalid ItemDataAsset"), __FUNCTION__, __LINE__);
		return false;
	}

	if (!CanAddItem(InItemDataAsset))
	{
		return false;
	}

	// Create new item instance
	FInventoryItemInstance NewItem;
	NewItem.InitializeFromDataAsset(InItemDataAsset);

	// Add to fast array
	FInventoryItemEntry NewEntry(NewItem);
	OutItemIndex = InventoryList.Items.Add(NewEntry);

	// Mark for replication
	InventoryList.MarkItemDirty(InventoryList.Items[OutItemIndex]);
	MARK_PROPERTY_DIRTY_FROM_NAME(UFragmentedInventoryComponent, InventoryList, this);

	// Broadcast event
	OnItemAdded.Broadcast(NewItem, OutItemIndex);

	return true;
}

bool UFragmentedInventoryComponent::RemoveItemByIndex(int32 InItemIndex)
{
	if (!InventoryList.Items.IsValidIndex(InItemIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("%hs:%d - Invalid item index %d"), __FUNCTION__, __LINE__, InItemIndex);
		return false;
	}

	FInventoryItemInstance remotedItem = InventoryList.Items[InItemIndex].ItemInstance;

	// Call fragment cleanup
	if (const UItemDefinitionAsset* definition = remotedItem.GetItemDataAsset())
	{
		const TArray<TObjectPtr<UItemFragment_Base>>& fragments = definition->GetFragments();
		for (int32 i = 0; i < fragments.Num(); ++i)
		{
			if (IsValid(fragments[i]) && remotedItem.DynamicFragmentData.IsValidIndex(i))
			{
				fragments[i]->OnItemDestroyed(remotedItem.DynamicFragmentData[i]);
			}
		}
	}

	InventoryList.Items.RemoveAt(InItemIndex);
	InventoryList.MarkArrayDirty();
	MARK_PROPERTY_DIRTY_FROM_NAME(UFragmentedInventoryComponent, InventoryList, this);

	OnItemRemoved.Broadcast(remotedItem, InItemIndex);

	return true;
}

bool UFragmentedInventoryComponent::RemoveItemByID(const FGuid& InItemInstanceID)
{
	const int32 ItemIndex = FindItemIndexByID(InItemInstanceID);
	if (ItemIndex == INDEX_NONE)
	{
		return false;
	}

	return RemoveItemByIndex(ItemIndex);
}

FInventoryItemInstance* UFragmentedInventoryComponent::FindItemByID_CPP(const FGuid& InItemInstanceID)
{
	const int32 ItemIndex = FindItemIndexByID(InItemInstanceID);
	if (ItemIndex == INDEX_NONE)
	{
		return nullptr;
	}

	return &InventoryList.Items[ItemIndex].ItemInstance;
}

FInventoryItemInstance* UFragmentedInventoryComponent::GetItemByIndex_CPP(int32 InItemIndex)
{
	if (!InventoryList.Items.IsValidIndex(InItemIndex))
	{
		return nullptr;
	}

	return &InventoryList.Items[InItemIndex].ItemInstance;
}

bool UFragmentedInventoryComponent::FindItemByID_BP(const FGuid& InItemInstanceID, FInventoryItemInstance& OutInstance)
{
	FInventoryItemInstance* item = FindItemByID_CPP(InItemInstanceID);
	if (item == nullptr)
	{
		OutInstance = {};
		return false;
	}
	OutInstance = *item;
	return true;
}

bool UFragmentedInventoryComponent::GetItemByIndex_BP(int32 InItemIndex, FInventoryItemInstance& OutInstance)
{
	FInventoryItemInstance* item = GetItemByIndex_CPP(InItemIndex);
	if (item == nullptr)
	{
		OutInstance = {};
		return false;
	}
	OutInstance = *item;
	return true;
}

void UFragmentedInventoryComponent::MarkItemDirty(int32 InItemIndex)
{
	if (!InventoryList.Items.IsValidIndex(InItemIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("%hs:%d - Invalid item index %d"), __FUNCTION__, __LINE__, InItemIndex);
		return;
	}

	InventoryList.MarkItemDirty(InventoryList.Items[InItemIndex]);
	MARK_PROPERTY_DIRTY_FROM_NAME(UFragmentedInventoryComponent, InventoryList, this);
}

void UFragmentedInventoryComponent::MarkItemDirtyByID(const FGuid& InItemInstanceID)
{
	const int32 ItemIndex = FindItemIndexByID(InItemInstanceID);
	if (ItemIndex != INDEX_NONE)
	{
		MarkItemDirty(ItemIndex);
	}
}

bool UFragmentedInventoryComponent::CanAddItem(const UItemDefinitionAsset* InItemDataAsset) const
{
	if (MaxItemSlots > 0 && InventoryList.Items.Num() >= MaxItemSlots)
	{
		return false;
	}

	return true;
}

int32 UFragmentedInventoryComponent::FindItemIndexByID(const FGuid& InItemInstanceID) const
{
	for (int32 Index = 0; Index < InventoryList.Items.Num(); ++Index)
	{
		if (InventoryList.Items[Index].ItemInstance.ItemInstanceID == InItemInstanceID)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}
