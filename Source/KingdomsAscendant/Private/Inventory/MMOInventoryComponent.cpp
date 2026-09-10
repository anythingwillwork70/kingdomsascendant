#include "Inventory/MMOInventoryComponent.h"
#include "Characters/MMOCharacter.h"
#include "Net/UnrealNetwork.h"

UMMOInventoryComponent::UMMOInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicated(true);
}

void UMMOInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // NOTE (scaling limitation, documented not hidden): InventoryItems
    // replicates as a plain TArray<FMMOInventoryItem>, which resends the
    // whole array on any single-element change. Fine at the 20-slot cap this
    // vertical slice uses. If inventory size grows significantly post-slice,
    // convert to a FFastArraySerializer-based TArray so only changed entries
    // replicate.
    DOREPLIFETIME(UMMOInventoryComponent, InventoryItems);
    DOREPLIFETIME(UMMOInventoryComponent, EquippedItems);
    DOREPLIFETIME(UMMOInventoryComponent, ResourceWood);
    DOREPLIFETIME(UMMOInventoryComponent, ResourceStone);
    DOREPLIFETIME(UMMOInventoryComponent, ResourceIron);
}

void UMMOInventoryComponent::OnRep_InventoryItems() { OnInventoryChanged.Broadcast(); }
void UMMOInventoryComponent::OnRep_EquippedItems()  { OnInventoryChanged.Broadcast(); }
void UMMOInventoryComponent::OnRep_Resources()      { OnResourcesChanged.Broadcast(); }

// ============================================================================
// ITEM MANAGEMENT
// ============================================================================

bool UMMOInventoryComponent::AddItem(const FMMOInventoryItem& NewItem)
{
    if (GetOwnerRole() != ROLE_Authority)
    {
        UE_LOG(LogTemp, Warning, TEXT("AddItem called on non-authority - ignored"));
        return false;
    }

    if (IsFull())
    {
        UE_LOG(LogTemp, Warning, TEXT("AddItem: inventory full (%d/%d)"), InventoryItems.Num(), MaxInventorySlots);
        return false;
    }

    InventoryItems.Add(NewItem);
    OnItemAdded.Broadcast(NewItem);
    return true;
}

bool UMMOInventoryComponent::RemoveItem(int32 SlotIndex)
{
    if (GetOwnerRole() != ROLE_Authority) return false;

    if (!InventoryItems.IsValidIndex(SlotIndex)) return false;

    InventoryItems.RemoveAt(SlotIndex);
    OnItemRemoved.Broadcast(SlotIndex);
    return true;
}

void UMMOInventoryComponent::RequestEquipItem(int32 SlotIndex)
{
    // Client entry point. UFUNCTION(Server, ...) automatically routes this
    // call to the server when invoked on a client-owned actor - no manual
    // IsLocallyControlled()/ROLE_Authority gating needed here (see
    // MASTER_CODE_AUDIT.md Finding #1 for what happens when that pattern is
    // done by hand instead of via an RPC).
    Server_EquipItem(SlotIndex);
}

bool UMMOInventoryComponent::Server_EquipItem_Validate(int32 SlotIndex)
{
    // Permissive - only reject obviously malformed input, not gameplay
    // state that can legitimately race (see Finding #10 on _Validate usage).
    return SlotIndex >= 0;
}

void UMMOInventoryComponent::Server_EquipItem_Implementation(int32 SlotIndex)
{
    if (!InventoryItems.IsValidIndex(SlotIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("Server_EquipItem: invalid slot %d"), SlotIndex);
        return;
    }

    const FMMOInventoryItem& Item = InventoryItems[SlotIndex];

    if (!Item.IsEquippable())
    {
        UE_LOG(LogTemp, Warning, TEXT("Server_EquipItem: item '%s' is not equippable"), *Item.ItemName.ToString());
        return;
    }

    AMMOCharacter* OwnerChar = Cast<AMMOCharacter>(GetOwner());
    if (OwnerChar && !Item.CanEquip(OwnerChar->ClassTag))
    {
        UE_LOG(LogTemp, Warning, TEXT("Server_EquipItem: '%s' requires class '%s'"),
            *Item.ItemName.ToString(), *Item.ClassRequirement.ToString());
        return;
    }

    FMMOInventoryItem* SlotRef = GetEquipSlotRef(Item.EquipSlot);
    if (!SlotRef)
    {
        UE_LOG(LogTemp, Warning, TEXT("Server_EquipItem: no equipment slot mapping for EItemSlot %d"), (int32)Item.EquipSlot);
        return;
    }

    // Swap: whatever was previously equipped goes back into the inventory
    // slot the new item is leaving, so nothing is ever destroyed by equipping.
    const FMMOInventoryItem PreviouslyEquipped = *SlotRef;
    *SlotRef = Item;

    if (PreviouslyEquipped.EquipSlot != EItemSlot::None)
    {
        InventoryItems[SlotIndex] = PreviouslyEquipped;
    }
    else
    {
        InventoryItems.RemoveAt(SlotIndex);
    }

    OnItemEquipped.Broadcast(Item.EquipSlot, Item);
}

FMMOInventoryItem* UMMOInventoryComponent::GetEquipSlotRef(EItemSlot Slot)
{
    switch (Slot)
    {
        case EItemSlot::Head:     return &EquippedItems.Head;
        case EItemSlot::Chest:    return &EquippedItems.Chest;
        case EItemSlot::Hands:    return &EquippedItems.Hands;
        case EItemSlot::Legs:     return &EquippedItems.Legs;
        case EItemSlot::Feet:     return &EquippedItems.Feet;
        case EItemSlot::Weapon:   return &EquippedItems.Weapon;
        case EItemSlot::Offhand:  return &EquippedItems.Offhand;
        case EItemSlot::Trinket1: return &EquippedItems.Trinket1;
        case EItemSlot::Trinket2: return &EquippedItems.Trinket2;
        default:                  return nullptr;
    }
}

// ============================================================================
// STARTER GEAR
// ============================================================================

void UMMOInventoryComponent::GrantStarterGear(FGameplayTag ClassTag)
{
    if (GetOwnerRole() != ROLE_Authority) return;

    static const FGameplayTag Tag_Guardian = FGameplayTag::RequestGameplayTag(FName("Class.Guardian"), false);
    static const FGameplayTag Tag_Wizard   = FGameplayTag::RequestGameplayTag(FName("Class.Wizard"), false);

    if (ClassTag.MatchesTag(Tag_Guardian))
    {
        FMMOInventoryItem Sword;
        Sword.ItemID = 101;
        Sword.ItemName = FText::FromString(TEXT("Iron Shortsword"));
        Sword.ItemRarity = EItemRarity::Common;
        Sword.GearTier = 1;
        Sword.EquipSlot = EItemSlot::Weapon;
        Sword.DamageValue = 15;
        Sword.ClassRequirement = Tag_Guardian;
        AddItem(Sword);

        FMMOInventoryItem Shield;
        Shield.ItemID = 102;
        Shield.ItemName = FText::FromString(TEXT("Wooden Buckler"));
        Shield.ItemRarity = EItemRarity::Common;
        Shield.GearTier = 1;
        Shield.EquipSlot = EItemSlot::Offhand;
        Shield.ArmorValue = 10;
        Shield.ClassRequirement = Tag_Guardian;
        AddItem(Shield);

        FMMOInventoryItem Cuirass;
        Cuirass.ItemID = 103;
        Cuirass.ItemName = FText::FromString(TEXT("Leather Cuirass"));
        Cuirass.ItemRarity = EItemRarity::Common;
        Cuirass.GearTier = 1;
        Cuirass.EquipSlot = EItemSlot::Chest;
        Cuirass.ArmorValue = 15;
        Cuirass.ClassRequirement = Tag_Guardian;
        AddItem(Cuirass);
    }
    else if (ClassTag.MatchesTag(Tag_Wizard))
    {
        FMMOInventoryItem Staff;
        Staff.ItemID = 201;
        Staff.ItemName = FText::FromString(TEXT("Apprentice Staff"));
        Staff.ItemRarity = EItemRarity::Common;
        Staff.GearTier = 1;
        Staff.EquipSlot = EItemSlot::Weapon;
        Staff.DamageValue = 18;
        Staff.ClassRequirement = Tag_Wizard;
        AddItem(Staff);

        FMMOInventoryItem Robes;
        Robes.ItemID = 202;
        Robes.ItemName = FText::FromString(TEXT("Novice Robes"));
        Robes.ItemRarity = EItemRarity::Common;
        Robes.GearTier = 1;
        Robes.EquipSlot = EItemSlot::Chest;
        Robes.ArmorValue = 6;
        Robes.ClassRequirement = Tag_Wizard;
        AddItem(Robes);

        FMMOInventoryItem Focus;
        Focus.ItemID = 203;
        Focus.ItemName = FText::FromString(TEXT("Arcane Focus Crystal"));
        Focus.ItemRarity = EItemRarity::Common;
        Focus.GearTier = 1;
        Focus.EquipSlot = EItemSlot::Trinket1;
        Focus.DamageValue = 5;
        Focus.ClassRequirement = Tag_Wizard;
        AddItem(Focus);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("GrantStarterGear: unrecognized class tag '%s' - no gear granted"), *ClassTag.ToString());
    }
}

// ============================================================================
// RESOURCES
// ============================================================================

void UMMOInventoryComponent::AddResources(int32 Wood, int32 Stone, int32 Iron)
{
    if (GetOwnerRole() != ROLE_Authority) return;

    ResourceWood += FMath::Max(0, Wood);
    ResourceStone += FMath::Max(0, Stone);
    ResourceIron += FMath::Max(0, Iron);

    OnResourcesChanged.Broadcast();
}

bool UMMOInventoryComponent::ConsumeResources(int32 Wood, int32 Stone, int32 Iron)
{
    if (GetOwnerRole() != ROLE_Authority)
    {
        UE_LOG(LogTemp, Error, TEXT("ConsumeResources called on non-authority - ignored"));
        return false;
    }

    if (ResourceWood < Wood || ResourceStone < Stone || ResourceIron < Iron)
    {
        return false;
    }

    ResourceWood -= Wood;
    ResourceStone -= Stone;
    ResourceIron -= Iron;

    OnResourcesChanged.Broadcast();
    return true;
}
