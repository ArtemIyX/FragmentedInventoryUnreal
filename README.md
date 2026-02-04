# Fragmented Inventory System

A flexible, network-replicated inventory system for Unreal Engine 5 that uses a fragment-based architecture to define item capabilities and behaviors.

## Overview

The Fragmented Inventory System provides a modular approach to creating inventory systems in UE5. Instead of creating monolithic item classes, items are defined through composable fragments that add specific capabilities (stackability, durability, equipment stats, etc.). This system uses Unreal's Fast Array Serialization for efficient network replication.

### Key Features

- **Fragment-Based Architecture**: Define items through composable, reusable fragments
- **Network Replicated**: Built on Fast Array Serialization for optimal network performance
- **Slot-Based Inventory**: Fixed-size inventory with configurable slot types and restrictions
- **Dynamic Item Data**: Each item instance can maintain unique runtime state
- **Stackable Items**: Native support for item stacking with configurable limits
- **Flexible Slot Management**: Support for different slot types (General, Equipment, Hotbar, Ammo, Quest)
- **Blueprint Friendly**: Extensive Blueprint support with delegates for UI integration

---

## Quick Start Guide

### 1. Add the Component

Add `UFragmentedInventoryComponent` to your actor:

```cpp
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
TObjectPtr<UFragmentedInventoryComponent> InventoryComponent;
```

### 2. Create an Item Definition

Create a new `UItemDefinitionAsset` Data Asset:
- Right-click in Content Browser → Miscellaneous → Data Asset
- Select `ItemDefinitionAsset` as the class
- Configure the item's display properties (name, description, icon)
- Add fragments to define item capabilities

### 3. Add Fragments

Create and configure fragments in the Item Definition:
- **ItemFragment_Stackable**: Makes items stackable with a max stack size
- Create your own custom fragments by inheriting from `UItemFragment_Base`

### 4. Add Items to Inventory

```cpp
// Server-side only
int32 slotIndex;
bool success = InventoryComponent->AddItem(slotIndex, ItemDataAsset, 5);
```

### 5. Query Inventory

```cpp
// Count items
int32 count = InventoryComponent->CountItem(ItemDataAsset);

// Check if has items
bool hasItem = InventoryComponent->HasItem(ItemDataAsset, 3);

// Get slot data
const FInventorySlot& slot = InventoryComponent->GetSlot(0);
```

---

## Architecture Overview

### Core Components

```
ItemDefinitionAsset (Data Asset)
    ↓ defines
InventoryItemInstance (Runtime Data)
    ↓ stored in
InventorySlot (Container)
    ↓ managed by
InventorySlotList (Fast Array)
    ↓ owned by
FragmentedInventoryComponent (Actor Component)
```

---

## Detailed Documentation

### UItemDefinitionAsset

The data asset that defines what an item *is* - its static properties and capabilities.

**Key Properties:**
- `Fragments`: Array of fragment instances that define item capabilities
- `ItemDisplayName`: Human-readable name for UI
- `ItemDescription`: Item description for tooltips
- `ItemIcon`: Icon texture for inventory display

**Key Methods:**
- `GetMaxStackSize()`: Returns maximum stack size (1 if not stackable)
- `IsStackable()`: Checks if item has a Stackable fragment
- `GetFragment<T>()`: Template method to retrieve a specific fragment type
- `HasFragment(FragmentClass)`: Check if item has a specific fragment
- `GetFragmentIndex(FragmentClass)`: Get array index of a fragment (for dynamic data lookup)

**Usage Example:**
```cpp
// Query item properties
int32 maxStack = ItemDataAsset->GetMaxStackSize();
bool canStack = ItemDataAsset->IsStackable();

// Get fragment
UItemFragment_Stackable* stackable = ItemDataAsset->GetFragment<UItemFragment_Stackable>();
```

---

### UItemFragment_Base

Abstract base class for all item fragments. Fragments define specific capabilities and behaviors.

**Virtual Methods:**
- `GetDynamicDataStructType()`: Returns the UScriptStruct type for runtime data (pure virtual)
- `InitializeDynamicData(OutDynamicData)`: Initialize runtime data with default values
- `OnItemCreated(ItemInstance, DynamicData)`: Called when an item with this fragment is created
- `OnItemDestroyed(ItemInstance, DynamicData)`: Called when an item is destroyed
- `GetDebugString(DynamicData)`: Returns debug information string

**Creating Custom Fragments:**

```cpp
// Header file
USTRUCT(BlueprintType)
struct FMyFragmentDynamicData
{
    GENERATED_BODY()
    
    UPROPERTY()
    int32 Durability = 100;
};

UCLASS()
class UItemFragment_MyFragment : public UItemFragment_Base
{
    GENERATED_BODY()
    
public:
    virtual const UScriptStruct* GetDynamicDataStructType() const override
    {
        return FMyFragmentDynamicData::StaticStruct();
    }
    
    virtual void InitializeDynamicData(FInstancedStruct& OutDynamicData) const override
    {
        Super::InitializeDynamicData(OutDynamicData);
        // Initialize with default values
    }
    
    UPROPERTY(EditDefaultsOnly, Category = "MyFragment")
    int32 MaxDurability = 100;
};
```

---

### UItemFragment_Stackable

Built-in fragment that enables item stacking.

**Properties:**
- `MaxStackSize`: Maximum items per stack (1-9999)
- `bAllowStackingWithDifferentData`: Whether items with different dynamic data can stack

**Dynamic Data:**
- `FStackableDynamicData`: Currently minimal, reserved for future runtime stack data

---

### FInventoryItemInstance

Runtime representation of a single item instance with unique state.

**Key Properties:**
- `ItemInstanceID`: Unique GUID for this specific instance
- `ItemDataAsset`: Soft reference to the item definition
- `DynamicFragmentData`: Array of runtime data for each fragment (indices match fragment array in ItemDataAsset)
- `ItemTags`: Optional gameplay tags for queries
- `CachedItemDataAsset`: Cached hard reference for fast access

**Key Methods:**
- `InitializeFromDataAsset(ItemDataAsset)`: Initialize instance from a data asset
- `GetFragmentDynamicData<T>(FragmentClass)`: Get mutable dynamic data for a fragment
- `GetFragmentDynamicData<T>(FragmentClass) const`: Get const dynamic data
- `IsValidData()`: Check if instance has valid ID
- `Reset()`: Clear the instance and call fragment cleanup

**Usage Example:**
```cpp
// Create instance
FInventoryItemInstance instance;
instance.InitializeFromDataAsset(ItemDataAsset);

// Access fragment dynamic data
FMyFragmentDynamicData* data = instance.GetFragmentDynamicData<FMyFragmentDynamicData>(
    UItemFragment_MyFragment::StaticClass()
);
if (data)
{
    data->Durability -= 10;
}
```

---

### FInventorySlot

Container for a single inventory slot with item instance and metadata.

**Key Properties:**
- `SlotIndex`: Fixed index in inventory
- `SlotType`: Type of slot (General, Equipment, Hotbar, Ammo, Quest)
- `SlotRestrictionTags`: Tags that restrict what items can be placed here
- `ItemInstance`: The item stored in this slot
- `CurrentStackSize`: Current number of items stacked
- `bIsLocked`: Whether slot can be modified

**Key Methods:**
- `IsEmpty()`: Check if slot has no item
- `CanAcceptItem(ItemDataAsset, Quantity)`: Check if slot can accept an item
- `CanStackWith(OtherItem)`: Check if can stack with another item instance
- `GetRemainingStackSpace()`: Get available stack space
- `GetMaxStackSize()`: Get max stack size for current item

---

### FInventorySlotList

Fast Array container for all inventory slots. Handles replication efficiently.

**Key Properties:**
- `Slots`: Array of inventory slots (what actually replicates)
- `OwnerComponent`: Reference to owning inventory component

**Key Methods:**
- `InitializeSlots(SlotCount, DefaultSlotType)`: Initialize all slots
- `GetSlot(SlotIndex)`: Get const slot reference
- `GetSlotMutable(SlotIndex)`: Get mutable slot reference
- `FindFirstEmptySlot(SlotType)`: Find first empty slot of type
- `FindSlotForItem(ItemDataAsset, Quantity)`: Find best slot for item (stackable or empty)

**Replication Callbacks:**
- `PreReplicatedRemove()`: Called before slots are removed
- `PostReplicatedAdd()`: Called after slots are added
- `PostReplicatedChange()`: Called after slots are modified (broadcasts slot changes)

---

### UFragmentedInventoryComponent

Main inventory component that manages all inventory operations.

**Configuration Properties:**
- `DefaultSlotCount`: Number of slots to initialize (default: 20)
- `DefaultSlotType`: Default slot type for initialization
- `bAutoInitialize`: Whether to initialize on BeginPlay

**Primary Methods:**

#### Adding Items
```cpp
// Add item (auto-finds slot, handles stacking)
bool AddItem(int32& OutSlotIndex, const UItemDefinitionAsset* ItemDataAsset, int32 Quantity = 1);

// Add with pre-configured instance (won't stack, creates unique items)
bool AddItemWithInstance(int32& OutSlotIndex, const FInventoryItemInstance& ItemInstance, int32 Quantity = 1);

// Add with callback to configure dynamic data
template<typename CallbackType>
bool AddItemWithCallback(int32& OutSlotIndex, const UItemDefinitionAsset* ItemDataAsset, 
                        int32 Quantity, CallbackType&& ConfigureCallback);

// Add to specific slot
bool AddItemToSlot(int32 SlotIndex, const UItemDefinitionAsset* ItemDataAsset, int32 Quantity = 1);
```

#### Removing Items
```cpp
// Remove from any slot with item
bool RemoveItem(const UItemDefinitionAsset* ItemDataAsset, int32 Quantity = 1);

// Remove from specific slot
bool RemoveItemFromSlot(int32 SlotIndex, int32 Quantity = 1);

// Clear entire slot
void ClearSlot(int32 SlotIndex);
```

#### Moving Items
```cpp
// Swap two slots completely
bool SwapSlots(int32 SlotIndexA, int32 SlotIndexB);

// Move items between slots (handles stacking)
// Use InQuantity = -1 to move all
bool MoveItem(int32 FromSlotIndex, int32 ToSlotIndex, int32 Quantity = -1);
```

#### Querying Inventory
```cpp
// Get slot data
const FInventorySlot& GetSlot(int32 SlotIndex) const;
bool IsValidSlot(int32 SlotIndex) const;
int32 GetSlotCount() const;

// Find slots
int32 FindFirstEmptySlot(EInventorySlotType SlotType = EInventorySlotType::General) const;

// Count items
int32 CountItem(const UItemDefinitionAsset* ItemDataAsset) const;
bool HasItem(const UItemDefinitionAsset* ItemDataAsset, int32 Quantity = 1) const;

// Inventory statistics
int32 GetTotalSlotCount() const;
int32 GetUsedSlotCount() const;
int32 GetEmptySlotCount() const;
float GetInventoryUsagePercent() const;
```

#### Slot Configuration
```cpp
// Configure slot properties (authority only)
void SetSlotType(int32 SlotIndex, EInventorySlotType SlotType);
void SetSlotRestrictionTags(int32 SlotIndex, const FGameplayTagContainer& RestrictionTags);
void SetSlotLocked(int32 SlotIndex, bool bLocked);
```

#### Accessing Fragment Data
```cpp
// Access item's fragment dynamic data
template <typename T>
T* GetSlotItemFragmentDynamicData(int32 SlotIndex, TSubclassOf<UItemFragment_Base> FragmentClass);
```

**Delegates:**
- `OnSlotChanged`: Broadcast when any slot changes
- `OnItemAdded`: Broadcast when items are added
- `OnItemRemoved`: Broadcast when items are removed  
- `OnSlotsSwapped`: Broadcast when slots are swapped

---

## Usage Examples

### Basic Item Operations

```cpp
// Add items to inventory
int32 slotIndex;
if (InventoryComponent->AddItem(slotIndex, HealthPotionAsset, 5))
{
    UE_LOG(LogTemp, Log, TEXT("Added 5 health potions to slot %d"), slotIndex);
}

// Remove items
if (InventoryComponent->RemoveItem(HealthPotionAsset, 1))
{
    UE_LOG(LogTemp, Log, TEXT("Used 1 health potion"));
}

// Check item count
int32 potionCount = InventoryComponent->CountItem(HealthPotionAsset);
```

### Working with Custom Fragment Data

```cpp
// Add item with custom durability
int32 slotIndex;
InventoryComponent->AddItemWithCallback(slotIndex, SwordAsset, 1, 
    [](FInventoryItemInstance& instance)
    {
        // Configure the item instance before it's added
        auto* durabilityData = instance.GetFragmentDynamicData<FDurabilityData>(
            UItemFragment_Durability::StaticClass()
        );
        if (durabilityData)
        {
            durabilityData->CurrentDurability = 50; // Damaged sword
        }
    }
);

// Later, access the durability data
auto* durabilityData = InventoryComponent->GetSlotItemFragmentDynamicData<FDurabilityData>(
    slotIndex, UItemFragment_Durability::StaticClass()
);
if (durabilityData)
{
    durabilityData->CurrentDurability -= 10;
}
```

### UI Integration with Delegates

```cpp
// In your UI controller class
void AMyPlayerController::BeginPlay()
{
    Super::BeginPlay();
    
    if (UFragmentedInventoryComponent* inventory = GetPawn()->FindComponentByClass<UFragmentedInventoryComponent>())
    {
        // Bind to inventory changes
        inventory->OnSlotChanged.AddDynamic(this, &AMyPlayerController::OnInventorySlotChanged);
        inventory->OnItemAdded.AddDynamic(this, &AMyPlayerController::OnItemAdded);
        inventory->OnItemRemoved.AddDynamic(this, &AMyPlayerController::OnItemRemoved);
    }
}

void AMyPlayerController::OnInventorySlotChanged(int32 slotIndex, const FInventorySlot& slot)
{
    // Update UI for this slot
    if (InventoryWidgetComponent)
    {
        InventoryWidgetComponent->RefreshSlot(slotIndex);
    }
}
```

### Slot Restrictions

```cpp
// Create a weapons-only slot
FGameplayTagContainer weaponTags;
weaponTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Item.Type.Weapon")));
InventoryComponent->SetSlotRestrictionTags(0, weaponTags);
InventoryComponent->SetSlotType(0, EInventorySlotType::Equipment);

// Lock a slot (prevent modifications)
InventoryComponent->SetSlotLocked(0, true);
```

---

## Network Replication

### Authority Requirements

Most inventory operations should be executed with server authority:
- Adding items
- Removing items
- Moving/swapping items
- Configuring slots

Query operations can be performed on any machine:
- Getting slot data
- Counting items
- Checking if items exist

### Replication Flow

1. Client performs UI action (drag item, click button)
2. Client calls RPC to server
3. Server validates and performs operation
4. Fast Array Serialization sends delta to clients
5. `PostReplicatedChange` callback fires on clients
6. Delegates broadcast to update UI

### Example RPC

```cpp
// In your player controller or character
UFUNCTION(Server, Reliable)
void ServerMoveInventoryItem(int32 fromSlot, int32 toSlot, int32 quantity);

void AMyCharacter::ServerMoveInventoryItem_Implementation(int32 fromSlot, int32 toSlot, int32 quantity)
{
    if (UFragmentedInventoryComponent* inventory = FindComponentByClass<UFragmentedInventoryComponent>())
    {
        inventory->MoveItem(fromSlot, toSlot, quantity);
    }
}
```

---

## Best Practices

### 1. Fragment Design
- Keep fragments focused on single responsibilities
- Use dynamic data for runtime state that changes
- Use fragment properties (in the asset) for static configuration
- Implement `GetDebugString()` for easier debugging

### 2. Item Instances
- Use `AddItem()` for generic stackable items
- Use `AddItemWithCallback()` when you need to configure dynamic data
- Use `AddItemWithInstance()` when you have a pre-configured instance

### 3. Performance
- Cache frequently accessed item data assets
- Avoid iterating through all slots in Tick
- Use delegates to update UI reactively instead of polling
- Let Fast Array Serialization handle replication automatically

### 4. Validation
- Always check `IsValid()` on UObjects
- Check `GetOwnerRole() == ROLE_Authority` for authority-only operations
- Validate slot indices before accessing
- Check `CanAcceptItem()` before adding items

### 5. UI Updates
- Bind to component delegates for reactive updates
- Update only changed slots, not entire inventory
- Use slot indices to identify what changed
- Consider caching slot widget references

---

## License

Project is licensed under [MIT](LICENSE)
