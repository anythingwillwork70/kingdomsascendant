#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "MMOAttributeSet.generated.h"

// Standard GAS boilerplate accessor macro block.
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * Core combat attribute set for AMMOCharacter.
 *
 * NETWORKING NOTE (this is the single most common GAS mistake, and the
 * reason this file did not previously exist despite being referenced
 * everywhere): every replicated FGameplayAttributeData MUST have a matching
 * OnRep_* UFUNCTION registered via DOREPLIFETIME_CONDITION_NOTIFY(..,
 * REPNOTIFY_Always) in GetLifetimeReplicatedProps, and that OnRep_* function
 * MUST call GAMEPLAYATTRIBUTE_REPNOTIFY. Skipping either half of that pair
 * compiles fine and appears to work for the server/listen-host player, then
 * silently fails to update the attribute's internal "last known server
 * value" tracking on remote clients, which corrupts GAS's prediction/
 * clamping math the next time a GameplayEffect is applied. Every attribute
 * below follows the full pattern.
 */
UCLASS()
class MYMMO_API UMMOAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    UMMOAttributeSet();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

    // ========================================================================
    // HEALTH
    // ========================================================================
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Attributes|Vital")
    FGameplayAttributeData Health;
    ATTRIBUTE_ACCESSORS(UMMOAttributeSet, Health)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Attributes|Vital")
    FGameplayAttributeData MaxHealth;
    ATTRIBUTE_ACCESSORS(UMMOAttributeSet, MaxHealth)

    // ========================================================================
    // MANA (Wizard resource; Guardian's "Energy" reuses this pool in the
    // vertical slice rather than adding a second parallel attribute pair -
    // see InitializeAttributesForLevel for the per-class semantics)
    // ========================================================================
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Mana, Category = "Attributes|Resource")
    FGameplayAttributeData Mana;
    ATTRIBUTE_ACCESSORS(UMMOAttributeSet, Mana)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxMana, Category = "Attributes|Resource")
    FGameplayAttributeData MaxMana;
    ATTRIBUTE_ACCESSORS(UMMOAttributeSet, MaxMana)

    // ========================================================================
    // COMBAT
    // ========================================================================
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AttackPower, Category = "Attributes|Combat")
    FGameplayAttributeData AttackPower;
    ATTRIBUTE_ACCESSORS(UMMOAttributeSet, AttackPower)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ArmorValue, Category = "Attributes|Combat")
    FGameplayAttributeData ArmorValue;
    ATTRIBUTE_ACCESSORS(UMMOAttributeSet, ArmorValue)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ThreatModifier, Category = "Attributes|Combat")
    FGameplayAttributeData ThreatModifier;
    ATTRIBUTE_ACCESSORS(UMMOAttributeSet, ThreatModifier)

    /**
     * Applies class/spec-scaled base stats for a given character level.
     * Server-authoritative only (attribute changes made off the server never
     * replicate correctly). Safe to call again on level-up - it sets
     * absolute values rather than deltas.
     */
    void InitializeAttributesForLevel(int32 Level, FGameplayTag ClassTag, FGameplayTag SpecTag);

protected:
    UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_Mana(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_MaxMana(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_AttackPower(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_ArmorValue(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_ThreatModifier(const FGameplayAttributeData& OldValue);
};
