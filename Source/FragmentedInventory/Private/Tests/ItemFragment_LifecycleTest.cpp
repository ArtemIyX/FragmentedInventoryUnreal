#include "Tests/ItemFragment_LifecycleTest.h"

#include "Data/InventoryItemInstance.h"

const UScriptStruct* UItemFragment_LifecycleTest::GetDynamicDataStructType() const
{
	return FItemFragmentLifecycleTestData::StaticStruct();
}

void UItemFragment_LifecycleTest::InitializeDynamicData(FInstancedStruct& OutDynamicData) const
{
	OutDynamicData.InitializeAs<FItemFragmentLifecycleTestData>();
}

void UItemFragment_LifecycleTest::OnItemCreated(FInventoryItemInstance* ItemInstance, const FInstancedStruct& InDynamicData) const
{
	++CreatedCallbackCount;
}

void UItemFragment_LifecycleTest::OnItemDestroyed(FInventoryItemInstance* ItemInstance, const FInstancedStruct& InDynamicData) const
{
	++DestroyedCallbackCount;
	if (InDynamicData.GetScriptStruct() != GetDynamicDataStructType())
	{
		++InvalidDestroyedDynamicDataCount;
	}
}

const UScriptStruct* UItemFragment_LifecycleDerivedTest::GetDynamicDataStructType() const
{
	return FItemFragmentLifecycleDerivedTestData::StaticStruct();
}

void UItemFragment_LifecycleDerivedTest::InitializeDynamicData(FInstancedStruct& OutDynamicData) const
{
	OutDynamicData.InitializeAs<FItemFragmentLifecycleDerivedTestData>();
}
