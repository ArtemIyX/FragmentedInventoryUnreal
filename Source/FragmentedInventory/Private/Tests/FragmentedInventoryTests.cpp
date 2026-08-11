#if WITH_DEV_AUTOMATION_TESTS

#include "Components/FragmentedInventoryComponent.h"
#include "Data/InventoryItemInstance.h"
#include "Data/InventorySlot.h"
#include "Data/ItemDefinitionAsset.h"
#include "Fragments/ItemFragment_Durability.h"
#include "Fragments/ItemFragment_Stackable.h"
#include "Fragments/ItemFragment_StackableConditional.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFragmentedInventorySlotCapacityTest,
	"FragmentedInventory.Inventory.SlotCapacity",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFragmentedInventorySlotCapacityTest::RunTest(const FString& Parameters)
{
	UItemDefinitionAsset* ItemDefinition = NewObject<UItemDefinitionAsset>();
	UItemFragment_Stackable* StackableFragment = NewObject<UItemFragment_Stackable>(ItemDefinition);
	StackableFragment->MaxStackSize = 10;
	ItemDefinition->Fragments.Add(StackableFragment);

	FInventorySlot Slot(0);
	TestTrue(TEXT("Empty slot accepts its maximum stack size"), Slot.CanAcceptItem(ItemDefinition, 10));
	TestFalse(TEXT("Empty slot rejects a quantity above its maximum stack size"), Slot.CanAcceptItem(ItemDefinition, 11));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFragmentedInventoryItemResetTest,
	"FragmentedInventory.Inventory.ItemReset",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFragmentedInventoryItemResetTest::RunTest(const FString& Parameters)
{
	UItemDefinitionAsset* ItemDefinition = NewObject<UItemDefinitionAsset>();
	FInventoryItemInstance ItemInstance;
	ItemInstance.InitializeFromDataAsset(ItemDefinition);
	ItemInstance.CachedItemDataAsset = nullptr;
	ItemInstance.Reset();

	TestFalse(TEXT("Reset clears an item whose transient asset cache is absent"), ItemInstance.IsValidData());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFragmentedInventoryFragmentInheritanceTest,
	"FragmentedInventory.Inventory.FragmentInheritance",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFragmentedInventoryFragmentInheritanceTest::RunTest(const FString& Parameters)
{
	UItemDefinitionAsset* ItemDefinition = NewObject<UItemDefinitionAsset>();
	UItemFragment_StackableConditional* ConditionalFragment = NewObject<UItemFragment_StackableConditional>(ItemDefinition);
	ItemDefinition->Fragments.Add(ConditionalFragment);

	TestEqual(
		TEXT("Base stackable lookup finds a conditional stackable fragment"),
		ItemDefinition->GetFragmentIndex(UItemFragment_Stackable::StaticClass()),
		0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFragmentedInventoryConditionalStackTest,
	"FragmentedInventory.Inventory.ConditionalStack",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFragmentedInventoryConditionalStackTest::RunTest(const FString& Parameters)
{
	AActor* Owner = NewObject<AActor>(GetTransientPackage());
	UFragmentedInventoryComponent* Inventory = NewObject<UFragmentedInventoryComponent>(Owner);
	Owner->AddOwnedComponent(Inventory);
	Inventory->InitializeInventory(2);

	UItemDefinitionAsset* ItemDefinition = NewObject<UItemDefinitionAsset>();
	ItemDefinition->Fragments.Add(NewObject<UItemFragment_StackableConditional>(ItemDefinition));

	int32 FirstSlot = INDEX_NONE;
	int32 SecondSlot = INDEX_NONE;
	TestTrue(TEXT("First conditional item is added"), Inventory->AddItem(FirstSlot, ItemDefinition));
	TestTrue(TEXT("Second conditional item is added"), Inventory->AddItem(SecondSlot, ItemDefinition));
	TestNotEqual(TEXT("Conditional items with unique keys do not merge"), FirstSlot, SecondSlot);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFragmentedInventoryRestrictedSwapTest,
	"FragmentedInventory.Inventory.RestrictedSwap",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFragmentedInventoryRestrictedSwapTest::RunTest(const FString& Parameters)
{
	const FGameplayTag InputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Local.Input")), false);
	const FGameplayTag MenuTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Local.Menu")), false);
	if (!InputTag.IsValid() || !MenuTag.IsValid())
	{
		AddError(TEXT("Required project gameplay tags are unavailable"));
		return false;
	}

	AActor* Owner = NewObject<AActor>(GetTransientPackage());
	UFragmentedInventoryComponent* Inventory = NewObject<UFragmentedInventoryComponent>(Owner);
	Owner->AddOwnedComponent(Inventory);
	Inventory->InitializeInventory(2);

	FGameplayTagContainer InputRestriction;
	InputRestriction.AddTag(InputTag);
	FGameplayTagContainer MenuRestriction;
	MenuRestriction.AddTag(MenuTag);
	Inventory->SetSlotRestrictionTags(0, InputRestriction);
	Inventory->SetSlotRestrictionTags(1, MenuRestriction);

	UItemDefinitionAsset* InputItem = NewObject<UItemDefinitionAsset>();
	InputItem->ItemTags.AddTag(InputTag);
	UItemDefinitionAsset* MenuItem = NewObject<UItemDefinitionAsset>();
	MenuItem->ItemTags.AddTag(MenuTag);

	TestTrue(TEXT("Input item enters input slot"), Inventory->AddItemToSlot(0, InputItem));
	TestTrue(TEXT("Menu item enters menu slot"), Inventory->AddItemToSlot(1, MenuItem));
	TestFalse(TEXT("Swap rejects incompatible destination restrictions"), Inventory->SwapSlots(0, 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFragmentedInventoryDynamicDataMutationTest,
	"FragmentedInventory.Inventory.DynamicDataMutation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFragmentedInventoryDynamicDataMutationTest::RunTest(const FString& Parameters)
{
	AActor* Owner = NewObject<AActor>(GetTransientPackage());
	UFragmentedInventoryComponent* Inventory = NewObject<UFragmentedInventoryComponent>(Owner);
	Owner->AddOwnedComponent(Inventory);
	Inventory->InitializeInventory(1);

	UItemDefinitionAsset* ItemDefinition = NewObject<UItemDefinitionAsset>();
	ItemDefinition->Fragments.Add(NewObject<UItemFragment_Durability>(ItemDefinition));
	int32 SlotIndex = INDEX_NONE;
	TestTrue(TEXT("Durability item is added"), Inventory->AddItem(SlotIndex, ItemDefinition));

	FInstancedStruct UpdatedData;
	UpdatedData.InitializeAs<FDurabilityDynamicData>();
	FDurabilityDynamicData* UpdatedDurabilityData = UpdatedData.GetMutablePtr<FDurabilityDynamicData>();
	TestNotNull(TEXT("Updated durability data is initialized"), UpdatedDurabilityData);
	if (UpdatedDurabilityData == nullptr)
	{
		return false;
	}
	UpdatedDurabilityData->CurrentDurability = 25.0f;
	TestTrue(TEXT("Dynamic data mutation is accepted on authority"), Inventory->SetSlotItemFragmentDynamicData(SlotIndex, UItemFragment_Durability::StaticClass(), UpdatedData));

	const FDurabilityDynamicData* DurabilityData = Inventory->GetSlotItemFragmentDynamicData<FDurabilityDynamicData>(SlotIndex, UItemFragment_Durability::StaticClass());
	TestNotNull(TEXT("Durability data remains readable"), DurabilityData);
	if (DurabilityData != nullptr)
	{
		TestEqual(TEXT("Authoritative dynamic data is updated"), DurabilityData->CurrentDurability, 25.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFragmentedInventoryAssetReadinessTest,
	"FragmentedInventory.Inventory.AssetReadiness",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFragmentedInventoryAssetReadinessTest::RunTest(const FString& Parameters)
{
	FInventoryItemInstance ItemInstance;
	ItemInstance.ItemInstanceID = FGuid::NewGuid();
	ItemInstance.ItemDataAsset = TSoftObjectPtr<UItemDefinitionAsset>(FSoftObjectPath(TEXT("/Game/FragmentedInventoryTests/UnloadedItem.UnloadedItem")));

	TestTrue(TEXT("Unloaded item retains a replicated definition path"), ItemInstance.IsValidData());
	TestFalse(TEXT("Unloaded item is not prediction-ready"), ItemInstance.IsItemDataAssetLoaded());
	TestTrue(TEXT("Unloaded item exposes a valid load path"), ItemInstance.GetItemDataAssetPath().IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFragmentedInventoryPredictionRollbackTest,
	"FragmentedInventory.Inventory.PredictionRollback",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FFragmentedInventoryPredictionRollbackTest::RunTest(const FString& Parameters)
{
	UFragmentedInventoryComponent* Inventory = NewObject<UFragmentedInventoryComponent>();
	Inventory->SlotList.InitializeSlots(2);

	UItemDefinitionAsset* ItemDefinition = NewObject<UItemDefinitionAsset>();
	FInventoryItemInstance ItemInstance;
	ItemInstance.InitializeFromDataAsset(ItemDefinition);
	Inventory->SlotList.Slots[0].ItemInstance = ItemInstance;
	Inventory->SlotList.Slots[0].CurrentStackSize = 1;

	UFragmentedInventoryComponent::FPendingMovePrediction Prediction;
	Prediction.PredictionId = 1;
	Prediction.FromSlotIndex = 0;
	Prediction.ToSlotIndex = 1;
	Prediction.FromSlotBefore = Inventory->SlotList.Slots[0];
	Prediction.ToSlotBefore = Inventory->SlotList.Slots[1];
	Inventory->PendingMovePrediction = Prediction;

	Inventory->SlotList.Slots[1].ItemInstance = Inventory->SlotList.Slots[0].ItemInstance;
	Inventory->SlotList.Slots[1].CurrentStackSize = 1;
	Inventory->SlotList.Slots[0].ItemInstance.Reset();
	Inventory->SlotList.Slots[0].CurrentStackSize = 0;
	Inventory->RollbackPendingMove();

	TestEqual(TEXT("Rollback restores source quantity"), Inventory->SlotList.Slots[0].CurrentStackSize, 1);
	TestTrue(TEXT("Rollback restores empty destination"), Inventory->SlotList.Slots[1].IsEmpty());
	TestFalse(TEXT("Raw move cannot mutate an unowned non-authoritative component"), Inventory->MoveItem(0, 1));
	return true;
}

#endif
