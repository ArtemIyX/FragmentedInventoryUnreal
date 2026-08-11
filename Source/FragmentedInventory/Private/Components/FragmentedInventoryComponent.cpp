#include "Components/FragmentedInventoryComponent.h"

#include "FragmentedInventory.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Actor.h"
#include "Logging/StructuredLog.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

UFragmentedInventoryComponent::UFragmentedInventoryComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, DefaultSlotCount(20)
	, DefaultSlotType(EInventorySlotType::General)
	, bAutoInitialize(true)
	, bUseNetworkPushModel(true)
	, InventoryRevision(0)
	, NextPredictionId(1)
	, ClientKnownInventoryRevision(0)
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

void UFragmentedInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetOwnerRole() == ROLE_Authority)
	{
		SlotList.ResetItemInstances();
	}

	PendingItemDefinitionLoads.Reset();
	PendingMovePrediction.Reset();
	Super::EndPlay(EndPlayReason);
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
		DOREPLIFETIME_WITH_PARAMS_FAST(UFragmentedInventoryComponent, InventoryRevision, Params);
		return;
	}

	DOREPLIFETIME(UFragmentedInventoryComponent, SlotList);
	DOREPLIFETIME(UFragmentedInventoryComponent, InventoryRevision);
}

void UFragmentedInventoryComponent::OnRep_InventoryRevision()
{
	ClientKnownInventoryRevision = InventoryRevision;
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
	CommitAuthorityMutation();

	for (int32 SlotIndex = 0; SlotIndex < SlotList.GetSlotCount(); ++SlotIndex)
	{
		BroadcastSlotChanged(SlotIndex);
	}
}

bool UFragmentedInventoryComponent::AddItem(int32& OutSlotIndex, const UItemDefinitionAsset* InItemDataAsset, int32 InQuantity)
{
	OutSlotIndex = INDEX_NONE;
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "AddItem called on non-authority");
		return false;
	}

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

	FInventoryItemInstance CandidateItemInstance = CreateItemInstance(InItemDataAsset, false);
	if (!CandidateItemInstance.IsValidData())
	{
		UE_LOGFMT(LogFragmentedInventory, Error, "Failed to create item instance for {ItemDefinition}", InItemDataAsset->GetName());
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
		else if (Slot->CanStackWith(CandidateItemInstance))
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
		CandidateItemInstance.Reset();
		return false;
	}

	TArray<int32> ChangedSlotIndices;
	TArray<int32> AddedQuantities;
	int32 RemainingQuantity = InQuantity;

	for (int32 SlotIndex = 0; SlotIndex < SlotList.GetSlotCount() && RemainingQuantity > 0; ++SlotIndex)
	{
		FInventorySlot* Slot = SlotList.GetSlotMutable(SlotIndex);
		if (Slot == nullptr || Slot->IsEmpty() || !Slot->CanStackWith(CandidateItemInstance))
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

	bool bUseCandidateItemInstance = true;
	while (RemainingQuantity > 0)
	{
		const int32 EmptySlotIndex = FindFirstEmptySlotForItem(InItemDataAsset);
		if (!ensureMsgf(EmptySlotIndex != INDEX_NONE, TEXT("Inventory capacity changed during AddItem")))
		{
			if (bUseCandidateItemInstance)
			{
				CandidateItemInstance.Reset();
			}
			return false;
		}

		FInventorySlot* Slot = SlotList.GetSlotMutable(EmptySlotIndex);
		if (!ensure(Slot != nullptr))
		{
			if (bUseCandidateItemInstance)
			{
				CandidateItemInstance.Reset();
			}
			return false;
		}

		const int32 QuantityToAdd = FMath::Min(RemainingQuantity, MaxStackSize);
		Slot->ItemInstance = bUseCandidateItemInstance ? MoveTemp(CandidateItemInstance) : CreateItemInstance(InItemDataAsset);
		if (bUseCandidateItemInstance)
		{
			Slot->ItemInstance.InvokeCreatedCallbacks();
		}
		Slot->CurrentStackSize = QuantityToAdd;
		RemainingQuantity -= QuantityToAdd;
		MarkSlotDirty(*Slot);
		ChangedSlotIndices.Add(EmptySlotIndex);
		AddedQuantities.Add(QuantityToAdd);
		OutSlotIndex = EmptySlotIndex;
		bUseCandidateItemInstance = false;
	}

	if (bUseCandidateItemInstance)
	{
		CandidateItemInstance.Reset();
	}

	CommitAuthorityMutation();
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
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "AddItemWithInstance called on non-authority");
		return false;
	}

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

		FInventoryItemInstance NewItemInstance;
		if (!NewItemInstance.InitializeFromExistingInstance(InItemInstance))
		{
			UE_LOGFMT(LogFragmentedInventory, Error, "Failed to clone item instance for {ItemDefinition}", ItemDataAsset->GetName());
			return false;
		}

		const int32 QuantityToAdd = FMath::Min(RemainingQuantity, MaxStackSize);
		Slot->ItemInstance = MoveTemp(NewItemInstance);
		Slot->CurrentStackSize = QuantityToAdd;
		RemainingQuantity -= QuantityToAdd;
		MarkSlotDirty(*Slot);
		ChangedSlotIndices.Add(EmptySlotIndex);
		AddedQuantities.Add(QuantityToAdd);
		OutSlotIndex = EmptySlotIndex;
	}

	CommitAuthorityMutation();
	for (int32 ChangeIndex = 0; ChangeIndex < ChangedSlotIndices.Num(); ++ChangeIndex)
	{
		BroadcastSlotChanged(ChangedSlotIndices[ChangeIndex]);
		OnItemAdded.Broadcast(ChangedSlotIndices[ChangeIndex], ItemDataAsset, AddedQuantities[ChangeIndex]);
	}

	return true;
}

bool UFragmentedInventoryComponent::AddItemToSlot(int32 InSlotIndex, const UItemDefinitionAsset* InItemDataAsset, int32 InQuantity)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "AddItemToSlot called on non-authority");
		return false;
	}

	if (!IsValid(InItemDataAsset) || InQuantity <= 0 || InItemDataAsset->GetMaxStackSize() <= 0)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "AddItemToSlot received invalid item definition or quantity {Quantity}", InQuantity);
		return false;
	}

	FInventorySlot* Slot = SlotList.GetSlotMutable(InSlotIndex);
	if (Slot == nullptr)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "Slot {SlotIndex} cannot accept the requested item quantity", InSlotIndex);
		return false;
	}

	if (Slot->IsEmpty())
	{
		if (!Slot->CanAcceptItem(InItemDataAsset, InQuantity))
		{
			UE_LOGFMT(LogFragmentedInventory, Warning, "Slot {SlotIndex} cannot accept the requested item quantity", InSlotIndex);
			return false;
		}

		Slot->ItemInstance = CreateItemInstance(InItemDataAsset);
		Slot->CurrentStackSize = InQuantity;
	}
	else
	{
		FInventoryItemInstance CandidateItemInstance = CreateItemInstance(InItemDataAsset, false);
		if (!Slot->CanAcceptItemInstance(CandidateItemInstance, InQuantity))
		{
			UE_LOGFMT(LogFragmentedInventory, Warning, "Slot {SlotIndex} cannot stack the requested item instance", InSlotIndex);
			CandidateItemInstance.Reset();
			return false;
		}

		Slot->CurrentStackSize += InQuantity;
		CandidateItemInstance.Reset();
	}

	MarkSlotDirty(*Slot);
	CommitAuthorityMutation();
	BroadcastSlotChanged(InSlotIndex);
	OnItemAdded.Broadcast(InSlotIndex, InItemDataAsset, InQuantity);
	return true;
}

bool UFragmentedInventoryComponent::AddItemToSlotWithInstance(int32 InSlotIndex, const FInventoryItemInstance& InItemInstance, int32 InQuantity)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "AddItemToSlotWithInstance called on non-authority");
		return false;
	}

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

	if (!Slot->ItemInstance.InitializeFromExistingInstance(InItemInstance))
	{
		UE_LOGFMT(LogFragmentedInventory, Error, "Failed to clone item instance for slot {SlotIndex}", InSlotIndex);
		return false;
	}
	Slot->CurrentStackSize = InQuantity;
	MarkSlotDirty(*Slot);
	CommitAuthorityMutation();
	BroadcastSlotChanged(InSlotIndex);
	OnItemAdded.Broadcast(InSlotIndex, ItemDataAsset, InQuantity);
	return true;
}

bool UFragmentedInventoryComponent::RemoveItem(const UItemDefinitionAsset* InItemDataAsset, int32 InQuantity)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "RemoveItem called on non-authority");
		return false;
	}

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

	CommitAuthorityMutation();
	for (int32 ChangeIndex = 0; ChangeIndex < ChangedSlotIndices.Num(); ++ChangeIndex)
	{
		BroadcastSlotChanged(ChangedSlotIndices[ChangeIndex]);
		OnItemRemoved.Broadcast(ChangedSlotIndices[ChangeIndex], InItemDataAsset, RemovedQuantities[ChangeIndex]);
	}

	return true;
}

bool UFragmentedInventoryComponent::RemoveItemFromSlot(int32 InSlotIndex, int32 InQuantity)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "RemoveItemFromSlot called on non-authority");
		return false;
	}

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

	CommitAuthorityMutation();
	BroadcastSlotChanged(InSlotIndex);
	OnItemRemoved.Broadcast(InSlotIndex, ItemDataAsset, InQuantity);
	return true;
}

void UFragmentedInventoryComponent::ClearSlot(int32 InSlotIndex)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "ClearSlot called on non-authority");
		return;
	}

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

	CommitAuthorityMutation();
	BroadcastSlotChanged(InSlotIndex);
	if (IsValid(ItemDataAsset) && Quantity > 0)
	{
		OnItemRemoved.Broadcast(InSlotIndex, ItemDataAsset, Quantity);
	}
}

bool UFragmentedInventoryComponent::SwapSlots(int32 InSlotIndexA, int32 InSlotIndexB)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "SwapSlots called on non-authority");
		return false;
	}

	TArray<int32> ChangedSlotIndices;
	const bool bSwapped = SwapSlotsInternal(InSlotIndexA, InSlotIndexB, &ChangedSlotIndices);
	if (bSwapped && InSlotIndexA != InSlotIndexB)
	{
		CommitAuthorityMutation();
		BroadcastSlotChanges(ChangedSlotIndices);
		OnSlotsSwapped.Broadcast(InSlotIndexA, InSlotIndexB);
	}

	return bSwapped;
}

bool UFragmentedInventoryComponent::SwapSlotsInternal(int32 InSlotIndexA, int32 InSlotIndexB, TArray<int32>* OutChangedSlotIndices)
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

	const UItemDefinitionAsset* ItemInSlotA = SlotA->ItemInstance.GetItemDataAsset();
	const UItemDefinitionAsset* ItemInSlotB = SlotB->ItemInstance.GetItemDataAsset();
	if ((!SlotA->IsEmpty() && !SlotB->CanPlaceItem(ItemInSlotA))
		|| (!SlotB->IsEmpty() && !SlotA->CanPlaceItem(ItemInSlotB)))
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "Slots {SlotIndexA} and {SlotIndexB} violate slot restrictions when swapped", InSlotIndexA, InSlotIndexB);
		return false;
	}

	Swap(SlotA->ItemInstance, SlotB->ItemInstance);
	Swap(SlotA->CurrentStackSize, SlotB->CurrentStackSize);
	MarkSlotDirty(*SlotA);
	MarkSlotDirty(*SlotB);
	if (OutChangedSlotIndices != nullptr)
	{
		OutChangedSlotIndices->Add(InSlotIndexA);
		OutChangedSlotIndices->Add(InSlotIndexB);
	}
	return true;
}

bool UFragmentedInventoryComponent::MoveItem(int32 InFromSlotIndex, int32 InToSlotIndex, int32 InQuantity)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "MoveItem called on non-authority. Use RequestMoveItem for a locally owned prediction.");
		return false;
	}

	TArray<int32> ChangedSlotIndices;
	bool bSlotsSwapped = false;
	const bool bMoved = MoveItemInternal(InFromSlotIndex, InToSlotIndex, InQuantity, &ChangedSlotIndices, &bSlotsSwapped);
	if (bMoved && InFromSlotIndex != InToSlotIndex)
	{
		CommitAuthorityMutation();
		BroadcastSlotChanges(ChangedSlotIndices);
		if (bSlotsSwapped)
		{
			OnSlotsSwapped.Broadcast(InFromSlotIndex, InToSlotIndex);
		}
	}

	return bMoved;
}

bool UFragmentedInventoryComponent::MoveItemInternal(int32 InFromSlotIndex, int32 InToSlotIndex, int32 InQuantity,
	TArray<int32>* OutChangedSlotIndices, bool* bOutSlotsSwapped)
{
	if (bOutSlotsSwapped != nullptr)
	{
		*bOutSlotsSwapped = false;
	}

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

		const bool bTransfersEntireSource = QuantityToMove == FromSlot->CurrentStackSize;
		if (bTransfersEntireSource)
		{
			ToSlot->ItemInstance = MoveTemp(FromSlot->ItemInstance);
		}
		else
		{
			const bool bInvokeCreatedCallbacks = GetOwnerRole() == ROLE_Authority;
			if (!ToSlot->ItemInstance.InitializeFromExistingInstance(FromSlot->ItemInstance, bInvokeCreatedCallbacks))
			{
				UE_LOGFMT(LogFragmentedInventory, Error, "Failed to clone item from slot {SlotIndex}", InFromSlotIndex);
				return false;
			}
		}

		ToSlot->CurrentStackSize = QuantityToMove;
		FromSlot->CurrentStackSize -= QuantityToMove;
		if (bTransfersEntireSource)
		{
			FromSlot->ItemInstance = FInventoryItemInstance();
			MarkSlotDirty(*FromSlot);
		}
		else
		{
			MarkSlotDirty(*FromSlot);
		}

		MarkSlotDirty(*ToSlot);
		if (OutChangedSlotIndices != nullptr)
		{
			OutChangedSlotIndices->Add(InFromSlotIndex);
			OutChangedSlotIndices->Add(InToSlotIndex);
		}
		return true;
	}

	if (ToSlot->CanStackWith(FromSlot->ItemInstance))
	{
		if (ToSlot->GetRemainingStackSpace() < QuantityToMove)
		{
			UE_LOGFMT(LogFragmentedInventory, Warning, "Target slot {SlotIndex} lacks capacity for requested quantity {Quantity}", InToSlotIndex, QuantityToMove);
			return false;
		}

		ToSlot->CurrentStackSize += QuantityToMove;
		FromSlot->CurrentStackSize -= QuantityToMove;
		if (FromSlot->CurrentStackSize == 0)
		{
			const UItemDefinitionAsset* RemovedItemDataAsset = nullptr;
			int32 ClearedQuantity = 0;
			const bool bInvokeDestroyedCallbacks = GetOwnerRole() == ROLE_Authority;
			if (!ensure(ClearSlotInternal(InFromSlotIndex, RemovedItemDataAsset, ClearedQuantity,
				bInvokeDestroyedCallbacks)))
			{
				return false;
			}
		}
		else
		{
			MarkSlotDirty(*FromSlot);
		}

		MarkSlotDirty(*ToSlot);
		if (OutChangedSlotIndices != nullptr)
		{
			OutChangedSlotIndices->Add(InFromSlotIndex);
			OutChangedSlotIndices->Add(InToSlotIndex);
		}
		return true;
	}

	const bool bSwapped = QuantityToMove == FromSlot->CurrentStackSize
		&& SwapSlotsInternal(InFromSlotIndex, InToSlotIndex, OutChangedSlotIndices);
	if (bOutSlotsSwapped != nullptr)
	{
		*bOutSlotsSwapped = bSwapped;
	}
	return bSwapped;
}

bool UFragmentedInventoryComponent::RequestMoveItem(int32 InFromSlotIndex, int32 InToSlotIndex, int32 InQuantity)
{
	if (GetOwnerRole() == ROLE_Authority)
	{
		return MoveItem(InFromSlotIndex, InToSlotIndex, InQuantity);
	}

	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasLocalNetOwner() || PendingMovePrediction.IsSet())
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "RequestMoveItem requires a locally owned inventory with no pending prediction");
		return false;
	}

	if (!AreMoveItemDefinitionsLoaded(InFromSlotIndex, InToSlotIndex))
	{
		UE_LOGFMT(LogFragmentedInventory, Verbose, "RequestMoveItem waits for replicated item definitions to load");
		return false;
	}

	const FInventorySlot* FromSlot = SlotList.GetSlot(InFromSlotIndex);
	const FInventorySlot* ToSlot = SlotList.GetSlot(InToSlotIndex);
	if (FromSlot == nullptr || ToSlot == nullptr)
	{
		return false;
	}

	FPendingMovePrediction NewPrediction;
	NewPrediction.PredictionId = NextPredictionId++;
	NewPrediction.FromSlotIndex = InFromSlotIndex;
	NewPrediction.ToSlotIndex = InToSlotIndex;
	NewPrediction.FromSlotBefore = *FromSlot;
	NewPrediction.ToSlotBefore = *ToSlot;
	NewPrediction.bAwaitingFromSlotRefresh = InFromSlotIndex != InToSlotIndex;
	NewPrediction.bAwaitingToSlotRefresh = InFromSlotIndex != InToSlotIndex;
	PendingMovePrediction = MoveTemp(NewPrediction);

	TArray<int32> ChangedSlotIndices;
	bool bSlotsSwapped = false;
	if (!MoveItemInternal(InFromSlotIndex, InToSlotIndex, InQuantity, &ChangedSlotIndices, &bSlotsSwapped))
	{
		PendingMovePrediction.Reset();
		return false;
	}

	BroadcastSlotChanges(ChangedSlotIndices);
	if (bSlotsSwapped)
	{
		OnSlotsSwapped.Broadcast(InFromSlotIndex, InToSlotIndex);
	}

	ServerRequestMoveItem(PendingMovePrediction->PredictionId, ClientKnownInventoryRevision, InFromSlotIndex, InToSlotIndex, InQuantity);
	return true;
}

void UFragmentedInventoryComponent::ServerRequestMoveItem_Implementation(int32 InPredictionId, int32 InBaseRevision, int32 InFromSlotIndex, int32 InToSlotIndex, int32 InQuantity)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	TArray<int32> ChangedSlotIndices;
	bool bSlotsSwapped = false;
	if (InBaseRevision != InventoryRevision || !MoveItemInternal(InFromSlotIndex, InToSlotIndex, InQuantity, &ChangedSlotIndices, &bSlotsSwapped))
	{
		ForceAuthoritativeSlotRefresh(InFromSlotIndex);
		ForceAuthoritativeSlotRefresh(InToSlotIndex);
		ClientRejectMoveItem(InPredictionId, InventoryRevision);
		return;
	}

	if (InFromSlotIndex != InToSlotIndex)
	{
		CommitAuthorityMutation();
		BroadcastSlotChanges(ChangedSlotIndices);
		if (bSlotsSwapped)
		{
			OnSlotsSwapped.Broadcast(InFromSlotIndex, InToSlotIndex);
		}
	}
	ClientAcknowledgeMoveItem(InPredictionId, InventoryRevision);
}

void UFragmentedInventoryComponent::ClientAcknowledgeMoveItem_Implementation(int32 InPredictionId, int32 InServerRevision)
{
	ClientKnownInventoryRevision = InServerRevision;
	if (!PendingMovePrediction.IsSet() || PendingMovePrediction->PredictionId != InPredictionId)
	{
		return;
	}

	if (PendingMovePrediction->FromSlotIndex == PendingMovePrediction->ToSlotIndex)
	{
		PendingMovePrediction.Reset();
	}
}

void UFragmentedInventoryComponent::ClientRejectMoveItem_Implementation(int32 InPredictionId, int32 InServerRevision)
{
	ClientKnownInventoryRevision = InServerRevision;
	if (!PendingMovePrediction.IsSet() || PendingMovePrediction->PredictionId != InPredictionId)
	{
		return;
	}

	RollbackPendingMove();
	if (PendingMovePrediction->FromSlotIndex == PendingMovePrediction->ToSlotIndex)
	{
		PendingMovePrediction.Reset();
	}
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
	CommitAuthorityMutation();
	BroadcastSlotChanged(InSlotIndex);
}

bool UFragmentedInventoryComponent::SetSlotRestrictionTags(int32 InSlotIndex, const FGameplayTagContainer& InRestrictionTags)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "SetSlotRestrictionTags called on non-authority");
		return false;
	}

	FInventorySlot* Slot = SlotList.GetSlotMutable(InSlotIndex);
	if (Slot == nullptr)
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "SetSlotRestrictionTags received invalid slot {SlotIndex}", InSlotIndex);
		return false;
	}

	FInventorySlot CandidateSlot = *Slot;
	CandidateSlot.SlotRestrictionTags = InRestrictionTags;
	if (!Slot->IsEmpty() && !CandidateSlot.CanPlaceItem(Slot->ItemInstance.GetItemDataAsset()))
	{
		UE_LOGFMT(LogFragmentedInventory, Warning, "Slot {SlotIndex} rejects the proposed restriction tags for its current item", InSlotIndex);
		return false;
	}

	Slot->SlotRestrictionTags = InRestrictionTags;
	MarkSlotDirty(*Slot);
	CommitAuthorityMutation();
	BroadcastSlotChanged(InSlotIndex);
	return true;
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
	CommitAuthorityMutation();
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

bool UFragmentedInventoryComponent::SetSlotItemFragmentDynamicData(int32 InSlotIndex, TSubclassOf<UItemFragment_Base> InFragmentClass,
	const FInstancedStruct& InDynamicData)
{
	if (GetOwnerRole() != ROLE_Authority || !InFragmentClass || !InDynamicData.IsValid())
	{
		return false;
	}

	FInventorySlot* Slot = SlotList.GetSlotMutable(InSlotIndex);
	if (Slot == nullptr || Slot->IsEmpty())
	{
		return false;
	}

	const int32 FragmentIndex = Slot->ItemInstance.GetFragmentIndex(InFragmentClass);
	if (!Slot->ItemInstance.DynamicFragmentData.IsValidIndex(FragmentIndex)
		|| Slot->ItemInstance.DynamicFragmentData[FragmentIndex].GetScriptStruct() != InDynamicData.GetScriptStruct())
	{
		return false;
	}

	Slot->ItemInstance.DynamicFragmentData[FragmentIndex] = InDynamicData;
	MarkSlotDirty(*Slot);
	CommitAuthorityMutation();
	BroadcastSlotChanged(InSlotIndex);
	return true;
}

void UFragmentedInventoryComponent::HandleReplicatedSlotChange(int32 InSlotIndex, const FInventorySlot& InSlot)
{
	EnsureItemDefinitionLoaded(InSlot.ItemInstance);
	OnSlotChanged.Broadcast(InSlotIndex, InSlot);
	ResolvePendingMoveIfRefreshed(InSlotIndex);
}

void UFragmentedInventoryComponent::BroadcastSlotChanged(int32 InSlotIndex)
{
	const FInventorySlot* Slot = SlotList.GetSlot(InSlotIndex);
	if (Slot != nullptr)
	{
		OnSlotChanged.Broadcast(InSlotIndex, *Slot);
	}
}

void UFragmentedInventoryComponent::BroadcastSlotChanges(const TArray<int32>& InSlotIndices)
{
	for (const int32 SlotIndex : InSlotIndices)
	{
		BroadcastSlotChanged(SlotIndex);
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

void UFragmentedInventoryComponent::CommitAuthorityMutation()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	InventoryRevision = InventoryRevision == MAX_int32 ? 1 : InventoryRevision + 1;
	if (bUseNetworkPushModel)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UFragmentedInventoryComponent, SlotList, this);
		MARK_PROPERTY_DIRTY_FROM_NAME(UFragmentedInventoryComponent, InventoryRevision, this);
	}
}

void UFragmentedInventoryComponent::ForceAuthoritativeSlotRefresh(int32 InSlotIndex)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	if (FInventorySlot* Slot = SlotList.GetSlotMutable(InSlotIndex))
	{
		MarkSlotDirty(*Slot);
	}
}

bool UFragmentedInventoryComponent::ClearSlotInternal(int32 InSlotIndex, const UItemDefinitionAsset*& OutItemDataAsset, int32& OutQuantity,
	bool bInInvokeDestroyedCallbacks)
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
	if (bInInvokeDestroyedCallbacks)
	{
		Slot->ItemInstance.Reset();
	}
	else
	{
		Slot->ItemInstance = FInventoryItemInstance();
	}
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

FInventoryItemInstance UFragmentedInventoryComponent::CreateItemInstance(const UItemDefinitionAsset* InItemDataAsset,
	bool bInInvokeCreatedCallbacks) const
{
	FInventoryItemInstance NewInstance;
	if (IsValid(InItemDataAsset))
	{
		NewInstance.InitializeFromDataAsset(InItemDataAsset, bInInvokeCreatedCallbacks);
	}

	return NewInstance;
}

void UFragmentedInventoryComponent::EnsureItemDefinitionLoaded(const FInventoryItemInstance& InItemInstance)
{
	if (InItemInstance.IsItemDataAssetLoaded())
	{
		return;
	}

	const FSoftObjectPath ItemDefinitionPath = InItemInstance.GetItemDataAssetPath();
	if (!ItemDefinitionPath.IsValid() || PendingItemDefinitionLoads.Contains(ItemDefinitionPath))
	{
		return;
	}

	TSharedPtr<FStreamableHandle> LoadHandle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
		ItemDefinitionPath,
		FStreamableDelegate::CreateWeakLambda(this, [this, ItemDefinitionPath]()
		{
			PendingItemDefinitionLoads.Remove(ItemDefinitionPath);
			for (int32 SlotIndex = 0; SlotIndex < SlotList.GetSlotCount(); ++SlotIndex)
			{
				const FInventorySlot* Slot = SlotList.GetSlot(SlotIndex);
				if (Slot != nullptr && Slot->ItemInstance.GetItemDataAssetPath() == ItemDefinitionPath)
				{
					BroadcastSlotChanged(SlotIndex);
				}
			}
		}));

	if (LoadHandle.IsValid())
	{
		PendingItemDefinitionLoads.Add(ItemDefinitionPath, MoveTemp(LoadHandle));
	}
}

bool UFragmentedInventoryComponent::AreMoveItemDefinitionsLoaded(int32 InFromSlotIndex, int32 InToSlotIndex)
{
	const FInventorySlot* FromSlot = SlotList.GetSlot(InFromSlotIndex);
	const FInventorySlot* ToSlot = SlotList.GetSlot(InToSlotIndex);
	if (FromSlot == nullptr || ToSlot == nullptr)
	{
		return false;
	}

	EnsureItemDefinitionLoaded(FromSlot->ItemInstance);
	EnsureItemDefinitionLoaded(ToSlot->ItemInstance);
	return FromSlot->ItemInstance.IsItemDataAssetLoaded()
		&& (ToSlot->IsEmpty() || ToSlot->ItemInstance.IsItemDataAssetLoaded());
}

void UFragmentedInventoryComponent::RollbackPendingMove()
{
	if (!PendingMovePrediction.IsSet())
	{
		return;
	}

	FPendingMovePrediction& Prediction = PendingMovePrediction.GetValue();
	FInventorySlot* FromSlot = SlotList.GetSlotMutable(Prediction.FromSlotIndex);
	FInventorySlot* ToSlot = SlotList.GetSlotMutable(Prediction.ToSlotIndex);
	if (FromSlot == nullptr || ToSlot == nullptr)
	{
		return;
	}

	*FromSlot = Prediction.FromSlotBefore;
	*ToSlot = Prediction.ToSlotBefore;
	BroadcastSlotChanged(Prediction.FromSlotIndex);
	BroadcastSlotChanged(Prediction.ToSlotIndex);
}

void UFragmentedInventoryComponent::ResolvePendingMoveIfRefreshed(int32 InSlotIndex)
{
	if (!PendingMovePrediction.IsSet())
	{
		return;
	}

	FPendingMovePrediction& Prediction = PendingMovePrediction.GetValue();
	if (InSlotIndex == Prediction.FromSlotIndex)
	{
		Prediction.bAwaitingFromSlotRefresh = false;
	}
	if (InSlotIndex == Prediction.ToSlotIndex)
	{
		Prediction.bAwaitingToSlotRefresh = false;
	}

	if (!Prediction.bAwaitingFromSlotRefresh && !Prediction.bAwaitingToSlotRefresh)
	{
		PendingMovePrediction.Reset();
	}
}
