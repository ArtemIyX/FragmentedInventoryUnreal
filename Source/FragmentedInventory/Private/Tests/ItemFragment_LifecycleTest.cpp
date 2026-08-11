#include "Tests/ItemFragment_LifecycleTest.h"

#include "Data/InventoryItemInstance.h"

const UScriptStruct* UItemFragment_LifecycleTest::GetDynamicDataStructType() const
{
	return nullptr;
}

void UItemFragment_LifecycleTest::InitializeDynamicData(FInstancedStruct& OutDynamicData) const
{
	OutDynamicData.Reset();
}

void UItemFragment_LifecycleTest::OnItemCreated(FInventoryItemInstance* ItemInstance, const FInstancedStruct& InDynamicData) const
{
	++CreatedCallbackCount;
}

void UItemFragment_LifecycleTest::OnItemDestroyed(FInventoryItemInstance* ItemInstance, const FInstancedStruct& InDynamicData) const
{
	++DestroyedCallbackCount;
}
