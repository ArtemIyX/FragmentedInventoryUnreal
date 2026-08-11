#pragma once

#include "CoreMinimal.h"
#include "Objects/ItemFragment_Base.h"
#include "ItemFragment_LifecycleTest.generated.h"

UCLASS()
class FRAGMENTEDINVENTORY_API UItemFragment_LifecycleTest : public UItemFragment_Base
{
	GENERATED_BODY()

public:
	virtual const UScriptStruct* GetDynamicDataStructType() const override;

	virtual void InitializeDynamicData(FInstancedStruct& OutDynamicData) const override;

	virtual void OnItemCreated(FInventoryItemInstance* ItemInstance, const FInstancedStruct& InDynamicData) const override;

	virtual void OnItemDestroyed(FInventoryItemInstance* ItemInstance, const FInstancedStruct& InDynamicData) const override;

	int32 GetCreatedCallbackCount() const { return CreatedCallbackCount; }

	int32 GetDestroyedCallbackCount() const { return DestroyedCallbackCount; }

private:
	mutable int32 CreatedCallbackCount = 0;

	mutable int32 DestroyedCallbackCount = 0;
};
