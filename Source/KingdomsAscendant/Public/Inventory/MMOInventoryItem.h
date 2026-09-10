#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MMOInventoryItem.generated.h"

UENUM(BlueprintType)
enum class EItemRarity : uint8
{
    Common = 0 UMETA(DisplayName = "Common (Grey)"),
    Uncommon = 1 UMETA(DisplayName = "Uncommon (Green)"),
    Rare = 2 UMETA(DisplayName = "Rare (Blue)"),
    Epic = 3 UMETA(DisplayName = "Epic (Purple)"),
    Legendary = 4 UMETA(DisplayName = "Legendary (Orange)")
};

UENUM(BlueprintType)
enum class EItemSlot : uint8
{
    Head,
    Chest,
    Hands,
    Legs,
    Feet,
    Weapon,
    Offhand,
    Trinket1,
    Trinket2,
    None
};

USTRUCT(BlueprintType)
struct FMMOInventoryItem
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite, Category = "Item")
    int32 ItemID = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Item")
    FText ItemName;

    UPROPERTY(BlueprintReadWrite, Category = "Item")
    FText ItemDescription;

    UPROPERTY(BlueprintReadWrite, Category = "Item")
    EItemRarity ItemRarity = EItemRarity::Common;

    UPROPERTY(BlueprintReadWrite, Category = "Item")
    int32 GearTier = 1;

    UPROPERTY(BlueprintReadWrite, Category = "Item")
    EItemSlot EquipSlot = EItemSlot::None;

    UPROPERTY(BlueprintReadWrite, Category = "Item")
    int32 ArmorValue = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Item")
    int32 DamageValue = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Item")
    FGameplayTag ClassRequirement;

    bool IsEquippable() const
    {
        return EquipSlot != EItemSlot::None;
    }

    bool CanEquip(FGameplayTag ClassTag) const
    {
        if (!ClassRequirement.IsValid())
        {
            return true;
        }
        return ClassTag.MatchesTag(ClassRequirement);
    }
};

USTRUCT(BlueprintType)
struct FMMOEquippedItems
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite, Category = "Equipment")
    FMMOInventoryItem Head;

    UPROPERTY(BlueprintReadWrite, Category = "Equipment")
    FMMOInventoryItem Chest;

    UPROPERTY(BlueprintReadWrite, Category = "Equipment")
    FMMOInventoryItem Hands;

    UPROPERTY(BlueprintReadWrite, Category = "Equipment")
    FMMOInventoryItem Legs;

    UPROPERTY(BlueprintReadWrite, Category = "Equipment")
    FMMOInventoryItem Feet;

    UPROPERTY(BlueprintReadWrite, Category = "Equipment")
    FMMOInventoryItem Weapon;

    UPROPERTY(BlueprintReadWrite, Category = "Equipment")
    FMMOInventoryItem Offhand;

    UPROPERTY(BlueprintReadWrite, Category = "Equipment")
    FMMOInventoryItem Trinket1;

    UPROPERTY(BlueprintReadWrite, Category = "Equipment")
    FMMOInventoryItem Trinket2;

    int32 GetTotalArmorValue() const
    {
        return Head.ArmorValue + Chest.ArmorValue + Hands.ArmorValue +
               Legs.ArmorValue + Feet.ArmorValue + Offhand.ArmorValue;
    }

    int32 GetTotalDamageValue() const
    {
        return Weapon.DamageValue + Offhand.DamageValue + Trinket1.DamageValue + Trinket2.DamageValue;
    }
};
