#if WITH_DEV_AUTOMATION_TESTS

#include "Data/InventoryItemInstance.h"
#include "Data/InventorySlot.h"
#include "Data/ItemDefinitionAsset.h"
#include "Fragments/ItemFragment_Stackable.h"
#include "Fragments/ItemFragment_StackableConditional.h"
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

#endif
