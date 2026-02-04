// Fill out your copyright notice in the Description page of Project Settings.

#include "Components/FragmentedInventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/ActorChannel.h"

UFragmentedInventoryComponent::UFragmentedInventoryComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	  , DefaultSlotCount(20)
	  , DefaultSlotType(EInventorySlotType::General)
	  , bAutoInitialize(true)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UFragmentedInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	// Initialize inventory on authority
	if (GetOwnerRole() == ROLE_Authority && bAutoInitialize)
	{
		InitializeInventory(DefaultSlotCount, DefaultSlotType);
	}
}

void UFragmentedInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UFragmentedInventoryComponent, SlotList);
}

void UFragmentedInventoryComponent::InitializeInventory(int32 InSlotCount, EInventorySlotType InDefaultSlotType)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("%hs:%d - InitializeInventory called on non-authority"), __FUNCTION__, __LINE__);
		return;
	}

	SlotList.OwnerComponent = this;
	SlotList.InitializeSlots(InSlotCount, InDefaultSlotType);

	UE_LOG(LogTemp, Log, TEXT("%hs:%d - Initialized inventory with %d slots"), __FUNCTION__, __LINE__, InSlotCount);
}

bool UFragmentedInventoryComponent::AddItem(int32& OutSlotIndex, const UItemDefinitionAsset* InItemDataAsset, int32 InQuantity)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("%hs:%d - AddItem called on non-authority"), __FUNCTION__, __LINE__);
		return false;
	}

	if (!IsValid(InItemDataAsset))
	{
		UE_LOG(LogTemp, Error, TEXT("%hs:%d - Invalid ItemDataAsset"), __FUNCTION__, __LINE__);
		return false;
	}

	if (InQuantity <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("%hs:%d - Invalid quantity: %d"), __FUNCTION__, __LINE__, InQuantity);
		return false;
	}

	int32 remainingQuantity = InQuantity;

	// Try to stack with existing items first
	for (int32 slotIndex = 0; slotIndex < SlotList.GetSlotCount(); ++slotIndex)
	{
		FInventorySlot* slot = SlotList.GetSlotMutable(slotIndex);
		if (slot == nullptr)
		{
			continue;
		}

		// Skip empty slots for now
		if (slot->IsEmpty())
		{
			continue;
		}

		// Check if we can stack
		if (slot->CanAcceptItem(InItemDataAsset, remainingQuantity))
		{
			const int32 spaceAvailable = slot->GetRemainingStackSpace();
			const int32 quantityToAdd = FMath::Min(remainingQuantity, spaceAvailable);

			slot->CurrentStackSize += quantityToAdd;
			remainingQuantity -= quantityToAdd;

			// Mark for replication
			SlotList.MarkItemDirty(*slot);
			BroadcastSlotChanged(slotIndex);
			OnItemAdded.Broadcast(slotIndex, InItemDataAsset, quantityToAdd);

			if (remainingQuantity <= 0)
			{
				OutSlotIndex = slotIndex;
				return true;
			}
		}
	}

	// Add remaining quantity to empty slots
	while (remainingQuantity > 0)
	{
		const int32 emptySlotIndex = SlotList.FindFirstEmptySlot(EInventorySlotType::General);
		if (emptySlotIndex == INDEX_NONE)
		{
			UE_LOG(LogTemp, Warning, TEXT("%hs:%d - No empty slots available"), __FUNCTION__, __LINE__);
			OutSlotIndex = INDEX_NONE;
			return false;
		}

		FInventorySlot* slot = SlotList.GetSlotMutable(emptySlotIndex);
		if (slot == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("%hs:%d - Failed to get slot at index %d"), __FUNCTION__, __LINE__,
			       emptySlotIndex);
			return false;
		}

		// Create new item instance
		slot->ItemInstance = CreateItemInstance(InItemDataAsset);

		const int32 quantityToAdd = FMath::Min(remainingQuantity, slot->MaxStackSize);
		slot->CurrentStackSize = quantityToAdd;
		remainingQuantity -= quantityToAdd;

		// Mark for replication
		SlotList.MarkItemDirty(*slot);
		BroadcastSlotChanged(emptySlotIndex);
		OnItemAdded.Broadcast(emptySlotIndex, InItemDataAsset, quantityToAdd);

		OutSlotIndex = emptySlotIndex;
	}

	return true;
}

bool UFragmentedInventoryComponent::AddItemToSlot(int32 InSlotIndex, const UItemDefinitionAsset* InItemDataAsset,
                                                  int32 InQuantity)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("%hs:%d - AddItemToSlot called on non-authority"), __FUNCTION__, __LINE__);
		return false;
	}

	FInventorySlot* slot = SlotList.GetSlotMutable(InSlotIndex);
	if (slot == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("%hs:%d - Invalid slot index: %d"), __FUNCTION__, __LINE__, InSlotIndex);
		return false;
	}

	if (!slot->CanAcceptItem(InItemDataAsset, InQuantity))
	{
		UE_LOG(LogTemp, Warning, TEXT("%hs:%d - Slot %d cannot accept item"), __FUNCTION__, __LINE__, InSlotIndex);
		return false;
	}

	// If slot is empty, create new instance
	if (slot->IsEmpty())
	{
		slot->ItemInstance = CreateItemInstance(InItemDataAsset);
		slot->CurrentStackSize = InQuantity;
	}
	else
	{
		// Stack with existing item
		slot->CurrentStackSize += InQuantity;
	}

	// Mark for replication
	SlotList.MarkItemDirty(*slot);
	BroadcastSlotChanged(InSlotIndex);
	OnItemAdded.Broadcast(InSlotIndex, InItemDataAsset, InQuantity);

	return true;
}

bool UFragmentedInventoryComponent::RemoveItem(const UItemDefinitionAsset* InItemDataAsset, int32 InQuantity)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("%hs:%d - RemoveItem called on non-authority"), __FUNCTION__, __LINE__);
		return false;
	}

	if (!IsValid(InItemDataAsset))
	{
		UE_LOG(LogTemp, Error, TEXT("%hs:%d - Invalid ItemDataAsset"), __FUNCTION__, __LINE__);
		return false;
	}

	int32 remainingQuantity = InQuantity;

	// Remove from slots that have this item
	for (int32 slotIndex = 0; slotIndex < SlotList.GetSlotCount(); ++slotIndex)
	{
		FInventorySlot* slot = SlotList.GetSlotMutable(slotIndex);
		if (slot == nullptr || slot->IsEmpty())
		{
			continue;
		}

		if (slot->ItemInstance.GetItemDataAsset() == InItemDataAsset)
		{
			const int32 quantityToRemove = FMath::Min(remainingQuantity, slot->CurrentStackSize);
			slot->CurrentStackSize -= quantityToRemove;
			remainingQuantity -= quantityToRemove;

			// If slot is now empty, clear it
			if (slot->CurrentStackSize <= 0)
			{
				ClearSlot(slotIndex);
			}
			else
			{
				SlotList.MarkItemDirty(*slot);
				BroadcastSlotChanged(slotIndex);
			}

			OnItemRemoved.Broadcast(slotIndex, InItemDataAsset, quantityToRemove);

			if (remainingQuantity <= 0)
			{
				return true;
			}
		}
	}

	// Couldn't remove full quantity
	UE_LOG(LogTemp, Warning, TEXT("%hs:%d - Could not remove full quantity. Remaining: %d"), __FUNCTION__, __LINE__,
	       remainingQuantity);
	return remainingQuantity == 0;
}

bool UFragmentedInventoryComponent::RemoveItemFromSlot(int32 InSlotIndex, int32 InQuantity)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("%hs:%d - RemoveItemFromSlot called on non-authority"), __FUNCTION__, __LINE__);
		return false;
	}

	FInventorySlot* slot = SlotList.GetSlotMutable(InSlotIndex);
	if (slot == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("%hs:%d - Invalid slot index: %d"), __FUNCTION__, __LINE__, InSlotIndex);
		return false;
	}

	if (slot->IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("%hs:%d - Slot %d is empty"), __FUNCTION__, __LINE__, InSlotIndex);
		return false;
	}

	if (slot->CurrentStackSize < InQuantity)
	{
		UE_LOG(LogTemp, Warning, TEXT("%hs:%d - Slot %d does not have enough items. Has: %d, Requested: %d"),
		       __FUNCTION__, __LINE__, InSlotIndex, slot->CurrentStackSize, InQuantity);
		return false;
	}

	const UItemDefinitionAsset* itemDataAsset = slot->ItemInstance.GetItemDataAsset();
	slot->CurrentStackSize -= InQuantity;

	if (slot->CurrentStackSize <= 0)
	{
		ClearSlot(InSlotIndex);
	}
	else
	{
		SlotList.MarkItemDirty(*slot);
		BroadcastSlotChanged(InSlotIndex);
	}

	OnItemRemoved.Broadcast(InSlotIndex, itemDataAsset, InQuantity);
	return true;
}

void UFragmentedInventoryComponent::ClearSlot(int32 InSlotIndex)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("%hs:%d - ClearSlot called on non-authority"), __FUNCTION__, __LINE__);
		return;
	}

	FInventorySlot* slot = SlotList.GetSlotMutable(InSlotIndex);
	if (slot == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("%hs:%d - Invalid slot index: %d"), __FUNCTION__, __LINE__, InSlotIndex);
		return;
	}

	const UItemDefinitionAsset* itemDataAsset = slot->ItemInstance.GetItemDataAsset();
	const int32 quantity = slot->CurrentStackSize;

	// Clear the slot
	slot->ItemInstance = FInventoryItemInstance();
	slot->CurrentStackSize = 0;

	// Mark for replication
	SlotList.MarkItemDirty(*slot);
	BroadcastSlotChanged(InSlotIndex);

	if (IsValid(itemDataAsset) && quantity > 0)
	{
		OnItemRemoved.Broadcast(InSlotIndex, itemDataAsset, quantity);
	}
}

bool UFragmentedInventoryComponent::SwapSlots(int32 InSlotIndexA, int32 InSlotIndexB)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("%hs:%d - SwapSlots called on non-authority"), __FUNCTION__, __LINE__);
		return false;
	}

	FInventorySlot* slotA = SlotList.GetSlotMutable(InSlotIndexA);
	FInventorySlot* slotB = SlotList.GetSlotMutable(InSlotIndexB);

	if (slotA == nullptr || slotB == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("%hs:%d - Invalid slot indices: %d, %d"), __FUNCTION__, __LINE__, InSlotIndexA,
		       InSlotIndexB);
		return false;
	}

	if (slotA->bIsLocked || slotB->bIsLocked)
	{
		UE_LOG(LogTemp, Warning, TEXT("%hs:%d - Cannot swap locked slots"), __FUNCTION__, __LINE__);
		return false;
	}

	// Swap the items
	FInventoryItemInstance tempInstance = slotA->ItemInstance;
	int32 tempStackSize = slotA->CurrentStackSize;

	slotA->ItemInstance = slotB->ItemInstance;
	slotA->CurrentStackSize = slotB->CurrentStackSize;

	slotB->ItemInstance = tempInstance;
	slotB->CurrentStackSize = tempStackSize;

	// Mark both for replication
	SlotList.MarkItemDirty(*slotA);
	SlotList.MarkItemDirty(*slotB);

	BroadcastSlotChanged(InSlotIndexA);
	BroadcastSlotChanged(InSlotIndexB);
	OnSlotsSwapped.Broadcast(InSlotIndexA, InSlotIndexB);

	return true;
}

bool UFragmentedInventoryComponent::MoveItem(int32 InFromSlotIndex, int32 InToSlotIndex, int32 InQuantity)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("%hs:%d - MoveItem called on non-authority"), __FUNCTION__, __LINE__);
		return false;
	}

	FInventorySlot* fromSlot = SlotList.GetSlotMutable(InFromSlotIndex);
	FInventorySlot* toSlot = SlotList.GetSlotMutable(InToSlotIndex);

	if (fromSlot == nullptr || toSlot == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("%hs:%d - Invalid slot indices: %d, %d"), __FUNCTION__, __LINE__, InFromSlotIndex,
		       InToSlotIndex);
		return false;
	}

	if (fromSlot->IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("%hs:%d - From slot %d is empty"), __FUNCTION__, __LINE__, InFromSlotIndex);
		return false;
	}

	// If quantity is -1, move all
	int32 quantityToMove = (InQuantity == -1) ? fromSlot->CurrentStackSize : InQuantity;

	if (quantityToMove > fromSlot->CurrentStackSize)
	{
		UE_LOG(LogTemp, Warning, TEXT("%hs:%d - Not enough items in from slot. Has: %d, Requested: %d"),
		       __FUNCTION__, __LINE__, fromSlot->CurrentStackSize, quantityToMove);
		return false;
	}

	const UItemDefinitionAsset* itemDataAsset = fromSlot->ItemInstance.GetItemDataAsset();

	// If target slot is empty, move items there
	if (toSlot->IsEmpty())
	{
		if (!toSlot->CanAcceptItem(itemDataAsset, quantityToMove))
		{
			UE_LOG(LogTemp, Warning, TEXT("%hs:%d - Target slot cannot accept item"), __FUNCTION__, __LINE__);
			return false;
		}

		toSlot->ItemInstance = fromSlot->ItemInstance;
		toSlot->CurrentStackSize = quantityToMove;
		fromSlot->CurrentStackSize -= quantityToMove;

		if (fromSlot->CurrentStackSize <= 0)
		{
			ClearSlot(InFromSlotIndex);
		}
		else
		{
			SlotList.MarkItemDirty(*fromSlot);
			BroadcastSlotChanged(InFromSlotIndex);
		}

		SlotList.MarkItemDirty(*toSlot);
		BroadcastSlotChanged(InToSlotIndex);
		return true;
	}

	// If target slot has the same item, try to stack
	if (toSlot->CanStackWith(fromSlot->ItemInstance))
	{
		const int32 spaceAvailable = toSlot->GetRemainingStackSpace();
		const int32 actualQuantityToMove = FMath::Min(quantityToMove, spaceAvailable);

		toSlot->CurrentStackSize += actualQuantityToMove;
		fromSlot->CurrentStackSize -= actualQuantityToMove;

		if (fromSlot->CurrentStackSize <= 0)
		{
			ClearSlot(InFromSlotIndex);
		}
		else
		{
			SlotList.MarkItemDirty(*fromSlot);
			BroadcastSlotChanged(InFromSlotIndex);
		}

		SlotList.MarkItemDirty(*toSlot);
		BroadcastSlotChanged(InToSlotIndex);
		return true;
	}

	// If target slot has different item, swap if moving all
	if (quantityToMove == fromSlot->CurrentStackSize)
	{
		return SwapSlots(InFromSlotIndex, InToSlotIndex);
	}

	UE_LOG(LogTemp, Warning, TEXT("%hs:%d - Cannot move partial stack to occupied slot with different item"),
	       __FUNCTION__, __LINE__);
	return false;
}

const FInventorySlot& UFragmentedInventoryComponent::GetSlot(int32 InSlotIndex) const
{
	static FInventorySlot emptySlot;

	const FInventorySlot* slot = SlotList.GetSlot(InSlotIndex);
	if (slot == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("%hs:%d - Invalid slot index: %d"), __FUNCTION__, __LINE__, InSlotIndex);
		return emptySlot;
	}

	return *slot;
}

bool UFragmentedInventoryComponent::IsValidSlot(int32 InSlotIndex) const
{
	return SlotList.GetSlot(InSlotIndex) != nullptr;
}

int32 UFragmentedInventoryComponent::FindFirstEmptySlot(EInventorySlotType InSlotType) const
{
	return SlotList.FindFirstEmptySlot(InSlotType);
}

int32 UFragmentedInventoryComponent::CountItem(const UItemDefinitionAsset* InItemDataAsset) const
{
	if (!IsValid(InItemDataAsset))
	{
		return 0;
	}

	int32 totalCount = 0;

	for (int32 slotIndex = 0; slotIndex < SlotList.GetSlotCount(); ++slotIndex)
	{
		const FInventorySlot* slot = SlotList.GetSlot(slotIndex);
		if (slot != nullptr && !slot->IsEmpty())
		{
			if (slot->ItemInstance.GetItemDataAsset() == InItemDataAsset)
			{
				totalCount += slot->CurrentStackSize;
			}
		}
	}

	return totalCount;
}

bool UFragmentedInventoryComponent::HasItem(const UItemDefinitionAsset* InItemDataAsset, int32 InQuantity) const
{
	return CountItem(InItemDataAsset) >= InQuantity;
}

void UFragmentedInventoryComponent::SetSlotType(int32 InSlotIndex, EInventorySlotType InSlotType)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("%hs:%d - SetSlotType called on non-authority"), __FUNCTION__, __LINE__);
		return;
	}

	FInventorySlot* slot = SlotList.GetSlotMutable(InSlotIndex);
	if (slot == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("%hs:%d - Invalid slot index: %d"), __FUNCTION__, __LINE__, InSlotIndex);
		return;
	}

	slot->SlotType = InSlotType;
	SlotList.MarkItemDirty(*slot);
	BroadcastSlotChanged(InSlotIndex);
}

void UFragmentedInventoryComponent::SetSlotRestrictionTags(int32 InSlotIndex,
                                                           const FGameplayTagContainer& InRestrictionTags)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("%hs:%d - SetSlotRestrictionTags called on non-authority"), __FUNCTION__,
		       __LINE__);
		return;
	}

	FInventorySlot* slot = SlotList.GetSlotMutable(InSlotIndex);
	if (slot == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("%hs:%d - Invalid slot index: %d"), __FUNCTION__, __LINE__, InSlotIndex);
		return;
	}

	slot->SlotRestrictionTags = InRestrictionTags;
	SlotList.MarkItemDirty(*slot);
	BroadcastSlotChanged(InSlotIndex);
}

void UFragmentedInventoryComponent::SetSlotLocked(int32 InSlotIndex, bool bInLocked)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOG(LogTemp, Warning, TEXT("%hs:%d - SetSlotLocked called on non-authority"), __FUNCTION__, __LINE__);
		return;
	}

	FInventorySlot* slot = SlotList.GetSlotMutable(InSlotIndex);
	if (slot == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("%hs:%d - Invalid slot index: %d"), __FUNCTION__, __LINE__, InSlotIndex);
		return;
	}

	slot->bIsLocked = bInLocked;
	SlotList.MarkItemDirty(*slot);
	BroadcastSlotChanged(InSlotIndex);
}

void UFragmentedInventoryComponent::BroadcastSlotChanged(int32 InSlotIndex)
{
	const FInventorySlot* slot = SlotList.GetSlot(InSlotIndex);
	if (slot != nullptr)
	{
		OnSlotChanged.Broadcast(InSlotIndex, *slot);
	}
}

FInventorySlot* UFragmentedInventoryComponent::GetSlotMutable(int32 InSlotIndex)
{
	return SlotList.GetSlotMutable(InSlotIndex);
}

FInventoryItemInstance UFragmentedInventoryComponent::CreateItemInstance(
	const UItemDefinitionAsset* InItemDataAsset) const
{
	FInventoryItemInstance newInstance;

	if (IsValid(InItemDataAsset))
	{
		newInstance.InitializeFromDataAsset(InItemDataAsset);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("%hs:%d - Cannot create item instance from invalid data asset"), __FUNCTION__,
		       __LINE__);
	}

	return newInstance;
}
