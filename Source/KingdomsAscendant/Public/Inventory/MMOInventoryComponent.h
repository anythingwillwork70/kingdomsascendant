#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Inventory/MMOInventoryItem.h"
#include "MMOInventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemAdded, const FMMOInventoryItem&, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemEquipped, EItemSlot, Slot, const FMMOInventoryItem&, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemRemoved, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnResourcesChanged);
/** Fired on every client (via OnRep) whenever the inventory array changes for ANY reason (add/remove/equip-swap). UI should re-read InventoryItems wholesale in response rather than assume a specific slot changed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MYMMO_API UMMOInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMMOInventoryComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ========================================================================
    // EQUIPMENT / ITEM INVENTORY
    // ========================================================================

    UPROPERTY(ReplicatedUsing = OnRep_InventoryItems, BlueprintReadOnly, Category = "Inventory")
    TArray<FMMOInventoryItem> InventoryItems;

    UPROPERTY(ReplicatedUsing = OnRep_EquippedItems, BlueprintReadOnly, Category = "Inventory")
    FMMOEquippedItems EquippedItems;

    UPROPERTY(EditDefaultsOnly, Category = "Inventory|Config")
    int32 MaxInventorySlots = 20;

    /** Server-authoritative. Returns false if inventory is full. */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool AddItem(const FMMOInventoryItem& NewItem);

    /** Server-authoritative. Removes and returns the item at SlotIndex. */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool RemoveItem(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool IsEmpty() const { return InventoryItems.Num() == 0; }

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool IsFull() const { return InventoryItems.Num() >= MaxInventorySlots; }

    /** Client-callable entry point (bind to UI). Routes to Server_EquipItem. */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void RequestEquipItem(int32 SlotIndex);

    UFUNCTION(Server, Reliable, WithValidation, Category = "Inventory")
    void Server_EquipItem(int32 SlotIndex);

    /** Grants the fixed starter loadout for a class. Server-authoritative, intended to be called exactly once per new character by AMMOCharacterSpawner. */
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void GrantStarterGear(FGameplayTag ClassTag);

    // ========================================================================
    // BUILDING RESOURCES
    // ========================================================================
    // AUDIT NOTE (see MASTER_CODE_AUDIT.md Finding #6): resources are a
    // separate stackable-currency concept, deliberately NOT modeled as
    // FMMOInventoryItem entries (which have no quantity field and would
    // exhaust the 20-slot cap after ~20 units). This mirrors how
    // FMMOPlayerProfile::AetherCurrency is already a plain int32, not an
    // inventory item.
    // ========================================================================

    UPROPERTY(ReplicatedUsing = OnRep_Resources, BlueprintReadOnly, Category = "Inventory|Resources")
    int32 ResourceWood = 0;

    UPROPERTY(ReplicatedUsing = OnRep_Resources, BlueprintReadOnly, Category = "Inventory|Resources")
    int32 ResourceStone = 0;

    UPROPERTY(ReplicatedUsing = OnRep_Resources, BlueprintReadOnly, Category = "Inventory|Resources")
    int32 ResourceIron = 0;

    UFUNCTION(BlueprintCallable, Category = "Inventory|Resources")
    int32 GetResourceWood() const { return ResourceWood; }

    UFUNCTION(BlueprintCallable, Category = "Inventory|Resources")
    int32 GetResourceStone() const { return ResourceStone; }

    UFUNCTION(BlueprintCallable, Category = "Inventory|Resources")
    int32 GetResourceIron() const { return ResourceIron; }

    /** Server-authoritative. Additive; used by gathering nodes/quest rewards. */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Resources")
    void AddResources(int32 Wood, int32 Stone, int32 Iron);

    /**
     * Server-authoritative. Atomically checks affordability THEN deducts all
     * three amounts in a single call - there is no separate check-then-remove
     * window for a second spend to race against. Returns false (and changes
     * nothing) if any single resource is insufficient.
     */
    UFUNCTION(BlueprintCallable, Category = "Inventory|Resources")
    bool ConsumeResources(int32 Wood, int32 Stone, int32 Iron);

    // ========================================================================
    // EVENTS
    // ========================================================================

    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnItemAdded OnItemAdded;

    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnItemEquipped OnItemEquipped;

    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnItemRemoved OnItemRemoved;

    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnResourcesChanged OnResourcesChanged;

    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnInventoryChanged OnInventoryChanged;

protected:
    UFUNCTION() void OnRep_InventoryItems();
    UFUNCTION() void OnRep_EquippedItems();
    UFUNCTION() void OnRep_Resources();

    /** Returns the equipment slot pointer on EquippedItems for a given EItemSlot, or nullptr for EItemSlot::None. */
    FMMOInventoryItem* GetEquipSlotRef(EItemSlot Slot);
};
