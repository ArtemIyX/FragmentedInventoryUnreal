#include "Components/FragmentedInventoryComponent.h"

#include "FragmentedInventory.h"
#include "Logging/StructuredLog.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

UFragmentedInventoryComponent::UFragmentedInventoryComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, DefaultSlotCount(20)
	, DefaultSlotType(EInventorySlotType::General)
	, bAutoInitialize(true)
	, bUseNetworkPushModel(true)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	SlotList.OwnerComponent = this;
}

void UFragmentedInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	SlotList.OwnerComponent = this;
	if (GetOwnerRole() == ROLE_Authority && bAutoInitialize)
	{
		InitializeInventory(DefaultSlotCount, DefaultSlotType);
	}
}

void UFragmentedInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	if (bUseNetworkPushModel)
	{
		FDoRepLifetimeParams Params;
		Params.bIsPushBased = true;
		Params.Condition = COND_None;
		Params.RepNotifyCondition = REPNOTIFY_OnChanged;
		DOREPLIFETIME_WITH_PARAMS_FAST(UFragmentedInventoryComponent, SlotList, Params);
		return;
	}

	DOREPLIFETIME(UFragmentedInventoryComponent, SlotList);
}

void UFragmentedInventoryComponent::InitializeInventory(int32 InSlotCount, EInventorySlotType InDefaultSlotType)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "InitializeInventory called on non-authority");
		return;
	}

	if (InSlotCount < 0)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "InitializeInventory received negative slot count {SlotCount}", InSlotCount);
		return;
	}

	SlotList.OwnerComponent = this;
	SlotList.InitializeSlots(InSlotCount, InDefaultSlotType);
	if (bUseNetworkPushModel)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UFragmentedInventoryComponent, SlotList, this);
	}

	for (int32 SlotIndex = 0; SlotIndex < SlotList.GetSlotCount(); ++SlotIndex)
	{
		BroadcastSlotChanged(SlotIndex);
	}
}

bool UFragmentedInventoryComponent::AddItem(int32& OutSlotIndex, const UItemDefinitionAsset* InItemDataAsset, int32 InQuantity)
{
	OutSlotIndex = INDEX_NONE;
	if (!IsValid(InItemDataAsset) || InQuantity <= 0)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "AddItem received invalid item definition or quantity {Quantity}", InQuantity);
		return false;
	}

	const int32 MaxStackSize = InItemDataAsset->GetMaxStackSize();
	if (MaxStackSize <= 0)
	{
		UE_LOGFMT(LogFragmentedInventory, Error, "Item definition {ItemDefinition} has invalid max stack size {MaxStackSize}", InItemDataAsset->GetName(), MaxStackSize);
		return false;
	}

	int64 AvailableCapacity = 0;
	for (int32 SlotIndex = 0; SlotIndex < SlotList.GetSlotCount(); ++SlotIndex)
	{
		const FInventorySlot* Slot = SlotList.GetSlot(SlotIndex);
		if (Slot == nullptr)
		{
			continue;
		}

		if (Slot->IsEmpty())
		{
			if (Slot->CanAcceptItem(InItemDataAsset, 1))
			{
				AvailableCapacity += MaxStackSize;
			}
		}
		else if (Slot->CanAcceptItem(InItemDataAsset, 1))
		{
			AvailableCapacity += FMath::Max(0, Slot->GetRemainingStackSpace());
		}

		if (AvailableCapacity >= InQuantity)
		{
			break;
		}
	}

	if (AvailableCapacity < InQuantity)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "Inventory lacks capacity for quantity {Quantity}", InQuantity);
		return false;
	}

	TArray<int32> ChangedSlotIndices;
	TArray<int32> AddedQuantities;
	int32 RemainingQuantity = InQuantity;

	for (int32 SlotIndex = 0; SlotIndex < SlotList.GetSlotCount() && RemainingQuantity > 0; ++SlotIndex)
	{
		FInventorySlot* Slot = SlotList.GetSlotMutable(SlotIndex);
		if (Slot == nullptr || Slot->IsEmpty() || !Slot->CanAcceptItem(InItemDataAsset, 1))
		{
			continue;
		}

		const int32 QuantityToAdd = FMath::Min(RemainingQuantity, Slot->GetRemainingStackSpace());
		if (QuantityToAdd <= 0)
		{
			continue;
		}

		Slot->CurrentStackSize += QuantityToAdd;
		RemainingQuantity -= QuantityToAdd;
		MarkSlotDirty(*Slot);
		ChangedSlotIndices.Add(SlotIndex);
		AddedQuantities.Add(QuantityToAdd);
		OutSlotIndex = SlotIndex;
	}

	while (RemainingQuantity > 0)
	{
		const int32 EmptySlotIndex = FindFirstEmptySlotForItem(InItemDataAsset);
		if (!ensureMsgf(EmptySlotIndex != INDEX_NONE, TEXT("Inventory capacity changed during AddItem")))
		{
			return false;
		}

		FInventorySlot* Slot = SlotList.GetSlotMutable(EmptySlotIndex);
		if (!ensure(Slot != nullptr))
		{
			return false;
		}

		const int32 QuantityToAdd = FMath::Min(RemainingQuantity, MaxStackSize);
		Slot->ItemInstance = CreateItemInstance(InItemDataAsset);
		Slot->CurrentStackSize = QuantityToAdd;
		RemainingQuantity -= QuantityToAdd;
		MarkSlotDirty(*Slot);
		ChangedSlotIndices.Add(EmptySlotIndex);
		AddedQuantities.Add(QuantityToAdd);
		OutSlotIndex = EmptySlotIndex;
	}

	for (int32 ChangeIndex = 0; ChangeIndex < ChangedSlotIndices.Num(); ++ChangeIndex)
	{
		BroadcastSlotChanged(ChangedSlotIndices[ChangeIndex]);
		OnItemAdded.Broadcast(ChangedSlotIndices[ChangeIndex], InItemDataAsset, AddedQuantities[ChangeIndex]);
	}

	return true;
}

bool UFragmentedInventoryComponent::AddItemWithInstance(int32& OutSlotIndex, const FInventoryItemInstance& InItemInstance, int32 InQuantity)
{
	OutSlotIndex = INDEX_NONE;
	const UItemDefinitionAsset* ItemDataAsset = InItemInstance.GetItemDataAsset();
	if (!InItemInstance.IsValidData() || !IsValid(ItemDataAsset) || InQuantity <= 0)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "AddItemWithInstance received invalid item instance or quantity {Quantity}", InQuantity);
		return false;
	}

	const int32 MaxStackSize = ItemDataAsset->GetMaxStackSize();
	if (MaxStackSize <= 0)
	{
		UE_LOGFMT(LogFragmentedInventory, Error, "Item definition {ItemDefinition} has invalid max stack size {MaxStackSize}", ItemDataAsset->GetName(), MaxStackSize);
		return false;
	}

	int64 AvailableCapacity = 0;
	for (int32 SlotIndex = 0; SlotIndex < SlotList.GetSlotCount(); ++SlotIndex)
	{
		const FInventorySlot* Slot = SlotList.GetSlot(SlotIndex);
		if (Slot != nullptr && Slot->IsEmpty() && Slot->CanAcceptItem(ItemDataAsset, 1))
		{
			AvailableCapacity += MaxStackSize;
		}
	}

	if (AvailableCapacity < InQuantity)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "Inventory lacks capacity for item instance quantity {Quantity}", InQuantity);
		return false;
	}

	TArray<int32> ChangedSlotIndices;
	TArray<int32> AddedQuantities;
	int32 RemainingQuantity = InQuantity;
	bool bUseOriginalInstance = true;

	while (RemainingQuantity > 0)
	{
		const int32 EmptySlotIndex = FindFirstEmptySlotForItem(ItemDataAsset);
		if (!ensureMsgf(EmptySlotIndex != INDEX_NONE, TEXT("Inventory capacity changed during AddItemWithInstance")))
		{
			return false;
		}

		FInventorySlot* Slot = SlotList.GetSlotMutable(EmptySlotIndex);
		if (!ensure(Slot != nullptr))
		{
			return false;
		}

		FInventoryItemInstance NewItemInstance = InItemInstance;
		if (!bUseOriginalInstance)
		{
			NewItemInstance.ItemInstanceID = FGuid::NewGuid();
		}

		const int32 QuantityToAdd = FMath::Min(RemainingQuantity, MaxStackSize);
		Slot->ItemInstance = MoveTemp(NewItemInstance);
		Slot->CurrentStackSize = QuantityToAdd;
		RemainingQuantity -= QuantityToAdd;
		MarkSlotDirty(*Slot);
		ChangedSlotIndices.Add(EmptySlotIndex);
		AddedQuantities.Add(QuantityToAdd);
		OutSlotIndex = EmptySlotIndex;
		bUseOriginalInstance = false;
	}

	for (int32 ChangeIndex = 0; ChangeIndex < ChangedSlotIndices.Num(); ++ChangeIndex)
	{
		BroadcastSlotChanged(ChangedSlotIndices[ChangeIndex]);
		OnItemAdded.Broadcast(ChangedSlotIndices[ChangeIndex], ItemDataAsset, AddedQuantities[ChangeIndex]);
	}

	return true;
}

bool UFragmentedInventoryComponent::AddItemToSlot(int32 InSlotIndex, const UItemDefinitionAsset* InItemDataAsset, int32 InQuantity)
{
	if (!IsValid(InItemDataAsset) || InQuantity <= 0 || InItemDataAsset->GetMaxStackSize() <= 0)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "AddItemToSlot received invalid item definition or quantity {Quantity}", InQuantity);
		return false;
	}

	FInventorySlot* Slot = SlotList.GetSlotMutable(InSlotIndex);
	if (Slot == nullptr || !Slot->CanAcceptItem(InItemDataAsset, InQuantity))
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "Slot {SlotIndex} cannot accept the requested item quantity", InSlotIndex);
		return false;
	}

	if (Slot->IsEmpty())
	{
		Slot->ItemInstance = CreateItemInstance(InItemDataAsset);
		Slot->CurrentStackSize = InQuantity;
	}
	else
	{
		Slot->CurrentStackSize += InQuantity;
	}

	MarkSlotDirty(*Slot);
	BroadcastSlotChanged(InSlotIndex);
	OnItemAdded.Broadcast(InSlotIndex, InItemDataAsset, InQuantity);
	return true;
}

bool UFragmentedInventoryComponent::AddItemToSlotWithInstance(int32 InSlotIndex, const FInventoryItemInstance& InItemInstance, int32 InQuantity)
{
	const UItemDefinitionAsset* ItemDataAsset = InItemInstance.GetItemDataAsset();
	if (!InItemInstance.IsValidData() || !IsValid(ItemDataAsset) || InQuantity <= 0 || ItemDataAsset->GetMaxStackSize() <= 0)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "AddItemToSlotWithInstance received invalid item instance or quantity {Quantity}", InQuantity);
		return false;
	}

	FInventorySlot* Slot = SlotList.GetSlotMutable(InSlotIndex);
	if (Slot == nullptr || !Slot->IsEmpty() || !Slot->CanAcceptItem(ItemDataAsset, InQuantity))
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "Slot {SlotIndex} cannot accept the requested item instance", InSlotIndex);
		return false;
	}

	Slot->ItemInstance = InItemInstance;
	Slot->CurrentStackSize = InQuantity;
	MarkSlotDirty(*Slot);
	BroadcastSlotChanged(InSlotIndex);
	OnItemAdded.Broadcast(InSlotIndex, ItemDataAsset, InQuantity);
	return true;
}

bool UFragmentedInventoryComponent::RemoveItem(const UItemDefinitionAsset* InItemDataAsset, int32 InQuantity)
{
	if (!IsValid(InItemDataAsset) || InQuantity <= 0)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "RemoveItem received invalid item definition or quantity {Quantity}", InQuantity);
		return false;
	}

	int64 RemovableQuantity = 0;
	for (int32 SlotIndex = 0; SlotIndex < SlotList.GetSlotCount(); ++SlotIndex)
	{
		const FInventorySlot* Slot = SlotList.GetSlot(SlotIndex);
		if (Slot != nullptr && !Slot->bIsLocked && !Slot->IsEmpty() && Slot->ItemInstance.IsItemDataAsset(InItemDataAsset))
		{
			RemovableQuantity += Slot->CurrentStackSize;
		}
	}

	if (RemovableQuantity < InQuantity)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "Inventory lacks removable quantity {Quantity}", InQuantity);
		return false;
	}

	TArray<int32> ChangedSlotIndices;
	TArray<int32> RemovedQuantities;
	int32 RemainingQuantity = InQuantity;
	for (int32 SlotIndex = 0; SlotIndex < SlotList.GetSlotCount() && RemainingQuantity > 0; ++SlotIndex)
	{
		FInventorySlot* Slot = SlotList.GetSlotMutable(SlotIndex);
		if (Slot == nullptr || Slot->bIsLocked || Slot->IsEmpty() || !Slot->ItemInstance.IsItemDataAsset(InItemDataAsset))
		{
			continue;
		}

		const int32 QuantityToRemove = FMath::Min(RemainingQuantity, Slot->CurrentStackSize);
		Slot->CurrentStackSize -= QuantityToRemove;
		RemainingQuantity -= QuantityToRemove;
		if (Slot->CurrentStackSize == 0)
		{
			const UItemDefinitionAsset* RemovedItemDataAsset = nullptr;
			int32 ClearedQuantity = 0;
			if (!ensure(ClearSlotInternal(SlotIndex, RemovedItemDataAsset, ClearedQuantity)))
			{
				return false;
			}
		}
		else
		{
			MarkSlotDirty(*Slot);
		}

		ChangedSlotIndices.Add(SlotIndex);
		RemovedQuantities.Add(QuantityToRemove);
	}

	for (int32 ChangeIndex = 0; ChangeIndex < ChangedSlotIndices.Num(); ++ChangeIndex)
	{
		BroadcastSlotChanged(ChangedSlotIndices[ChangeIndex]);
		OnItemRemoved.Broadcast(ChangedSlotIndices[ChangeIndex], InItemDataAsset, RemovedQuantities[ChangeIndex]);
	}

	return true;
}

bool UFragmentedInventoryComponent::RemoveItemFromSlot(int32 InSlotIndex, int32 InQuantity)
{
	if (InQuantity <= 0)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "RemoveItemFromSlot received invalid quantity {Quantity}", InQuantity);
		return false;
	}

	FInventorySlot* Slot = SlotList.GetSlotMutable(InSlotIndex);
	if (Slot == nullptr || Slot->bIsLocked || Slot->IsEmpty() || Slot->CurrentStackSize < InQuantity)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "Slot {SlotIndex} cannot remove quantity {Quantity}", InSlotIndex, InQuantity);
		return false;
	}

	const UItemDefinitionAsset* ItemDataAsset = Slot->ItemInstance.GetItemDataAsset();
	Slot->CurrentStackSize -= InQuantity;
	if (Slot->CurrentStackSize == 0)
	{
		const UItemDefinitionAsset* RemovedItemDataAsset = nullptr;
		int32 ClearedQuantity = 0;
		if (!ensure(ClearSlotInternal(InSlotIndex, RemovedItemDataAsset, ClearedQuantity)))
		{
			return false;
		}
	}
	else
	{
		MarkSlotDirty(*Slot);
	}

	BroadcastSlotChanged(InSlotIndex);
	OnItemRemoved.Broadcast(InSlotIndex, ItemDataAsset, InQuantity);
	return true;
}

void UFragmentedInventoryComponent::ClearSlot(int32 InSlotIndex)
{
	FInventorySlot* Slot = SlotList.GetSlotMutable(InSlotIndex);
	if (Slot == nullptr || Slot->bIsLocked)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "Slot {SlotIndex} cannot be cleared", InSlotIndex);
		return;
	}

	const UItemDefinitionAsset* ItemDataAsset = nullptr;
	int32 Quantity = 0;
	if (!ClearSlotInternal(InSlotIndex, ItemDataAsset, Quantity))
	{
		return;
	}

	BroadcastSlotChanged(InSlotIndex);
	if (IsValid(ItemDataAsset) && Quantity > 0)
	{
		OnItemRemoved.Broadcast(InSlotIndex, ItemDataAsset, Quantity);
	}
}

bool UFragmentedInventoryComponent::SwapSlots(int32 InSlotIndexA, int32 InSlotIndexB)
{
	FInventorySlot* SlotA = SlotList.GetSlotMutable(InSlotIndexA);
	FInventorySlot* SlotB = SlotList.GetSlotMutable(InSlotIndexB);
	if (SlotA == nullptr || SlotB == nullptr || SlotA->bIsLocked || SlotB->bIsLocked)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "Slots {SlotIndexA} and {SlotIndexB} cannot be swapped", InSlotIndexA, InSlotIndexB);
		return false;
	}

	if (InSlotIndexA == InSlotIndexB)
	{
		return true;
	}

	Swap(SlotA->ItemInstance, SlotB->ItemInstance);
	Swap(SlotA->CurrentStackSize, SlotB->CurrentStackSize);
	MarkSlotDirty(*SlotA);
	MarkSlotDirty(*SlotB);
	BroadcastSlotChanged(InSlotIndexA);
	BroadcastSlotChanged(InSlotIndexB);
	OnSlotsSwapped.Broadcast(InSlotIndexA, InSlotIndexB);
	return true;
}

bool UFragmentedInventoryComponent::MoveItem(int32 InFromSlotIndex, int32 InToSlotIndex, int32 InQuantity)
{
	if (InQuantity != -1 && InQuantity <= 0)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "MoveItem received invalid quantity {Quantity}", InQuantity);
		return false;
	}

	FInventorySlot* FromSlot = SlotList.GetSlotMutable(InFromSlotIndex);
	FInventorySlot* ToSlot = SlotList.GetSlotMutable(InToSlotIndex);
	if (FromSlot == nullptr || ToSlot == nullptr || FromSlot->bIsLocked || FromSlot->IsEmpty())
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "Slots {FromSlotIndex} and {ToSlotIndex} cannot move an item", InFromSlotIndex, InToSlotIndex);
		return false;
	}

	if (InFromSlotIndex == InToSlotIndex)
	{
		return true;
	}

	const int32 QuantityToMove = InQuantity == -1 ? FromSlot->CurrentStackSize : InQuantity;
	if (QuantityToMove > FromSlot->CurrentStackSize)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "Source slot {SlotIndex} lacks requested quantity {Quantity}", InFromSlotIndex, QuantityToMove);
		return false;
	}

	const UItemDefinitionAsset* ItemDataAsset = FromSlot->ItemInstance.GetItemDataAsset();
	if (ToSlot->IsEmpty())
	{
		if (!ToSlot->CanAcceptItem(ItemDataAsset, QuantityToMove))
		{
			return false;
		}

		ToSlot->ItemInstance = FromSlot->ItemInstance;
		if (QuantityToMove < FromSlot->CurrentStackSize)
		{
			ToSlot->ItemInstance.ItemInstanceID = FGuid::NewGuid();
		}

		ToSlot->CurrentStackSize = QuantityToMove;
		FromSlot->CurrentStackSize -= QuantityToMove;
		if (FromSlot->CurrentStackSize == 0)
		{
			const UItemDefinitionAsset* RemovedItemDataAsset = nullptr;
			int32 ClearedQuantity = 0;
			if (!ensure(ClearSlotInternal(InFromSlotIndex, RemovedItemDataAsset, ClearedQuantity)))
			{
				return false;
			}
		}
		else
		{
			MarkSlotDirty(*FromSlot);
		}

		MarkSlotDirty(*ToSlot);
		BroadcastSlotChanged(InFromSlotIndex);
		BroadcastSlotChanged(InToSlotIndex);
		return true;
	}

	if (ToSlot->CanStackWith(FromSlot->ItemInstance))
	{
		const int32 ActualQuantityToMove = FMath::Min(QuantityToMove, ToSlot->GetRemainingStackSpace());
		if (ActualQuantityToMove <= 0)
		{
			return false;
		}

		ToSlot->CurrentStackSize += ActualQuantityToMove;
		FromSlot->CurrentStackSize -= ActualQuantityToMove;
		if (FromSlot->CurrentStackSize == 0)
		{
			const UItemDefinitionAsset* RemovedItemDataAsset = nullptr;
			int32 ClearedQuantity = 0;
			if (!ensure(ClearSlotInternal(InFromSlotIndex, RemovedItemDataAsset, ClearedQuantity)))
			{
				return false;
			}
		}
		else
		{
			MarkSlotDirty(*FromSlot);
		}

		MarkSlotDirty(*ToSlot);
		BroadcastSlotChanged(InFromSlotIndex);
		BroadcastSlotChanged(InToSlotIndex);
		return true;
	}

	return QuantityToMove == FromSlot->CurrentStackSize && SwapSlots(InFromSlotIndex, InToSlotIndex);
}

const FInventorySlot& UFragmentedInventoryComponent::GetSlot(int32 InSlotIndex) const
{
	static const FInventorySlot EmptySlot;
	const FInventorySlot* Slot = SlotList.GetSlot(InSlotIndex);
	if (Slot == nullptr)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "Invalid slot index {SlotIndex}", InSlotIndex);
		return EmptySlot;
	}

	return *Slot;
}

bool UFragmentedInventoryComponent::IsValidSlot(int32 InSlotIndex) const
{
	return SlotList.GetSlot(InSlotIndex) != nullptr;
}

int32 UFragmentedInventoryComponent::FindFirstEmptySlot(EInventorySlotType InSlotType) const
{
	return SlotList.FindFirstEmptySlot(InSlotType);
}

int32 UFragmentedInventoryComponent::FindFirstEmptySlotAnyType() const
{
	return SlotList.FindFirstEmptySlot();
}

int32 UFragmentedInventoryComponent::CountItem(const UItemDefinitionAsset* InItemDataAsset) const
{
	if (!IsValid(InItemDataAsset))
	{
		return 0;
	}

	int32 TotalCount = 0;
	for (int32 SlotIndex = 0; SlotIndex < SlotList.GetSlotCount(); ++SlotIndex)
	{
		const FInventorySlot* Slot = SlotList.GetSlot(SlotIndex);
		if (Slot != nullptr && !Slot->IsEmpty() && Slot->ItemInstance.IsItemDataAsset(InItemDataAsset))
		{
			TotalCount += Slot->CurrentStackSize;
		}
	}

	return TotalCount;
}

bool UFragmentedInventoryComponent::HasItem(const UItemDefinitionAsset* InItemDataAsset, int32 InQuantity) const
{
	return InQuantity > 0 && CountItem(InItemDataAsset) >= InQuantity;
}

void UFragmentedInventoryComponent::SetSlotType(int32 InSlotIndex, EInventorySlotType InSlotType)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "SetSlotType called on non-authority");
		return;
	}

	FInventorySlot* Slot = SlotList.GetSlotMutable(InSlotIndex);
	if (Slot == nullptr)
	{
		return;
	}

	Slot->SlotType = InSlotType;
	MarkSlotDirty(*Slot);
	BroadcastSlotChanged(InSlotIndex);
}

void UFragmentedInventoryComponent::SetSlotRestrictionTags(int32 InSlotIndex, const FGameplayTagContainer& InRestrictionTags)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "SetSlotRestrictionTags called on non-authority");
		return;
	}

	FInventorySlot* Slot = SlotList.GetSlotMutable(InSlotIndex);
	if (Slot == nullptr)
	{
		return;
	}

	Slot->SlotRestrictionTags = InRestrictionTags;
	MarkSlotDirty(*Slot);
	BroadcastSlotChanged(InSlotIndex);
}

void UFragmentedInventoryComponent::SetSlotLocked(int32 InSlotIndex, bool bInLocked)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "SetSlotLocked called on non-authority");
		return;
	}

	FInventorySlot* Slot = SlotList.GetSlotMutable(InSlotIndex);
	if (Slot == nullptr)
	{
		return;
	}

	Slot->bIsLocked = bInLocked;
	MarkSlotDirty(*Slot);
	BroadcastSlotChanged(InSlotIndex);
}

int32 UFragmentedInventoryComponent::GetTotalSlotCount() const
{
	return SlotList.GetSlotCount();
}

int32 UFragmentedInventoryComponent::GetUsedSlotCount() const
{
	int32 UsedSlotCount = 0;
	for (int32 SlotIndex = 0; SlotIndex < SlotList.GetSlotCount(); ++SlotIndex)
	{
		const FInventorySlot* Slot = SlotList.GetSlot(SlotIndex);
		if (Slot != nullptr && !Slot->IsEmpty())
		{
			++UsedSlotCount;
		}
	}

	return UsedSlotCount;
}

int32 UFragmentedInventoryComponent::GetEmptySlotCount() const
{
	return GetTotalSlotCount() - GetUsedSlotCount();
}

float UFragmentedInventoryComponent::GetInventoryUsagePercent() const
{
	const int32 TotalSlotCount = GetTotalSlotCount();
	return TotalSlotCount > 0 ? static_cast<float>(GetUsedSlotCount()) / static_cast<float>(TotalSlotCount) : 0.0f;
}

void UFragmentedInventoryComponent::BroadcastSlotChanged(int32 InSlotIndex)
{
	const FInventorySlot* Slot = SlotList.GetSlot(InSlotIndex);
	if (Slot != nullptr)
	{
		OnSlotChanged.Broadcast(InSlotIndex, *Slot);
	}
}

void UFragmentedInventoryComponent::MarkSlotDirty(FInventorySlot& InSlot)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	SlotList.MarkItemDirty(InSlot);
	if (bUseNetworkPushModel)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UFragmentedInventoryComponent, SlotList, this);
	}
}

bool UFragmentedInventoryComponent::ClearSlotInternal(int32 InSlotIndex, const UItemDefinitionAsset*& OutItemDataAsset, int32& OutQuantity)
{
	OutItemDataAsset = nullptr;
	OutQuantity = 0;
	FInventorySlot* Slot = SlotList.GetSlotMutable(InSlotIndex);
	if (Slot == nullptr || Slot->IsEmpty())
	{
		return false;
	}

	OutItemDataAsset = Slot->ItemInstance.GetItemDataAsset();
	OutQuantity = Slot->CurrentStackSize;
	Slot->ItemInstance.Reset();
	Slot->CurrentStackSize = 0;
	MarkSlotDirty(*Slot);
	return true;
}

int32 UFragmentedInventoryComponent::FindFirstEmptySlotForItem(const UItemDefinitionAsset* InItemDataAsset) const
{
	for (int32 SlotIndex = 0; SlotIndex < SlotList.GetSlotCount(); ++SlotIndex)
	{
		const FInventorySlot* Slot = SlotList.GetSlot(SlotIndex);
		if (Slot != nullptr && Slot->IsEmpty() && Slot->CanAcceptItem(InItemDataAsset, 1))
		{
			return SlotIndex;
		}
	}

	return INDEX_NONE;
}

FInventorySlot* UFragmentedInventoryComponent::GetSlotMutable(int32 InSlotIndex)
{
	return SlotList.GetSlotMutable(InSlotIndex);
}

FInventoryItemInstance UFragmentedInventoryComponent::CreateItemInstance(const UItemDefinitionAsset* InItemDataAsset) const
{
	FInventoryItemInstance NewInstance;
	if (IsValid(InItemDataAsset))
	{
		NewInstance.InitializeFromDataAsset(InItemDataAsset);
	}

	return NewInstance;
}
