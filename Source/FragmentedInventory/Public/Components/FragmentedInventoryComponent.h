// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/InventoryItemInstance.h"
#include "Data/InventoryItemList.h"
#include "FragmentedInventoryComponent.generated.h"


class UItemDefinitionAsset;

UCLASS(Blueprintable, BlueprintType, ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class FRAGMENTEDINVENTORY_API UFragmentedInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFragmentedInventoryComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

private:
	// Find item index by ID
	int32 FindItemIndexByID(const FGuid& InItemInstanceID) const;

protected:
	// The replicated inventory list
	UPROPERTY(Replicated)
	FInventoryItemList InventoryList;

	// Maximum number of items (0 = unlimited)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Configuration")
	int32 MaxItemSlots = 20;

	// Can add item validation
	virtual bool CanAddItem(const UItemDefinitionAsset* InItemDataAsset) const;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch,
	                                 FReplicationFlags* RepFlags) override;

public:
	// Item management
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool AddItem(const UItemDefinitionAsset* InItemDataAsset, int32& OutItemIndex);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool RemoveItemByIndex(int32 InItemIndex);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool RemoveItemByID(const FGuid& InItemInstanceID);


	FInventoryItemInstance* FindItemByID_CPP(const FGuid& InItemInstanceID);
	FInventoryItemInstance* GetItemByIndex_CPP(int32 InItemIndex);

	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool FindItemByID_BP(const FGuid& InItemInstanceID, FInventoryItemInstance& OutInstance);

	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool GetItemByIndex_BP(int32 InItemIndex, FInventoryItemInstance& OutInstance);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	int32 GetItemCount() const { return InventoryList.Items.Num(); }

public:
	// Mark item as dirty for replication
	void MarkItemDirty(int32 InItemIndex);
	void MarkItemDirtyByID(const FGuid& InItemInstanceID);

public:
	// Events
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemAdded, const FInventoryItemInstance&, InItem, int32,
	                                             InItemIndex);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemRemoved, const FInventoryItemInstance&, InItem, int32,
	                                             InItemIndex);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemChanged, const FInventoryItemInstance&, InItem, int32,
	                                             InItemIndex);

	UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
	FOnItemAdded OnItemAdded;

	UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
	FOnItemRemoved OnItemRemoved;

	UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
	FOnItemChanged OnItemChanged;
};
