#include "AbilitySystem/MMOAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"

UMMOAttributeSet::UMMOAttributeSet()
{
    // Safe non-zero defaults so an un-initialized character (e.g. viewed in
    // the editor before InitializeAttributesForLevel ever runs) doesn't show
    // 0/0 health and read as already dead.
    Health.SetBaseValue(100.0f);
    Health.SetCurrentValue(100.0f);
    MaxHealth.SetBaseValue(100.0f);
    MaxHealth.SetCurrentValue(100.0f);
}

void UMMOAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // REPNOTIFY_Always: attribute changes must always trigger OnRep, even if
    // the raw replicated bytes happen to match the previous value (GAS's own
    // prediction/correction logic depends on OnRep firing every time the
    // server sends an update, not just when the bit pattern changes).
    DOREPLIFETIME_CONDITION_NOTIFY(UMMOAttributeSet, Health, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UMMOAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UMMOAttributeSet, Mana, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UMMOAttributeSet, MaxMana, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UMMOAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UMMOAttributeSet, ArmorValue, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UMMOAttributeSet, ThreatModifier, COND_None, REPNOTIFY_Always);
}

void UMMOAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)         { GAMEPLAYATTRIBUTE_REPNOTIFY(UMMOAttributeSet, Health, OldValue); }
void UMMOAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)      { GAMEPLAYATTRIBUTE_REPNOTIFY(UMMOAttributeSet, MaxHealth, OldValue); }
void UMMOAttributeSet::OnRep_Mana(const FGameplayAttributeData& OldValue)           { GAMEPLAYATTRIBUTE_REPNOTIFY(UMMOAttributeSet, Mana, OldValue); }
void UMMOAttributeSet::OnRep_MaxMana(const FGameplayAttributeData& OldValue)        { GAMEPLAYATTRIBUTE_REPNOTIFY(UMMOAttributeSet, MaxMana, OldValue); }
void UMMOAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldValue)    { GAMEPLAYATTRIBUTE_REPNOTIFY(UMMOAttributeSet, AttackPower, OldValue); }
void UMMOAttributeSet::OnRep_ArmorValue(const FGameplayAttributeData& OldValue)     { GAMEPLAYATTRIBUTE_REPNOTIFY(UMMOAttributeSet, ArmorValue, OldValue); }
void UMMOAttributeSet::OnRep_ThreatModifier(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UMMOAttributeSet, ThreatModifier, OldValue); }

void UMMOAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    // Clamp CURRENT values against their max as they change (covers direct
    // SetCurrentValue calls, e.g. from Blueprint or debug commands).
    if (Attribute == GetHealthAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
    }
    else if (Attribute == GetManaAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxMana());
    }
}

void UMMOAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    // Clamp again AFTER a GameplayEffect executes (covers effects that
    // modify Health/Mana via a Meta attribute or additive/multiplicative
    // modifiers rather than a direct SetCurrentValue, which PreAttributeChange
    // alone does not intercept).
    if (Data.EvaluatedData.Attribute == GetHealthAttribute())
    {
        SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
    }
    else if (Data.EvaluatedData.Attribute == GetManaAttribute())
    {
        SetMana(FMath::Clamp(GetMana(), 0.0f, GetMaxMana()));
    }
}

// ============================================================================
// LEVEL / CLASS / SPEC SCALING
// ============================================================================
// Base (Level 1) values below are chosen so that, run through LevelMultiplier
// at Level 20, they land inside the ranges established for the vertical
// slice: Guardian Health 1200-1500 / AttackPower 50-100 / ThreatModifier
// 1.0-3.5; Wizard Health 750-950 / Mana 1000+ / AttackPower 60-120, with
// Wizard ArmorValue deliberately lower than Guardian's.
// ============================================================================

void UMMOAttributeSet::InitializeAttributesForLevel(int32 Level, FGameplayTag ClassTag, FGameplayTag SpecTag)
{
    Level = FMath::Clamp(Level, 1, 20);

    // 1.0 at Level 1 scaling smoothly to 4.0 at Level 20.
    const float LevelMultiplier = 1.0f + (static_cast<float>(Level) - 1.0f) * 0.173f;

    static const FGameplayTag Tag_Guardian_Vanguard  = FGameplayTag::RequestGameplayTag(FName("Class.Guardian.Vanguard"), false);
    static const FGameplayTag Tag_Guardian_Berserker  = FGameplayTag::RequestGameplayTag(FName("Class.Guardian.Berserker"), false);
    static const FGameplayTag Tag_Wizard_Elementalist = FGameplayTag::RequestGameplayTag(FName("Class.Wizard.Elementalist"), false);
    static const FGameplayTag Tag_Wizard_BattleMage    = FGameplayTag::RequestGameplayTag(FName("Class.Wizard.BattleMage"), false);

    float BaseHealth = 250.0f;
    float BaseMana = 0.0f;
    float BaseAttackPower = 15.0f;
    float BaseArmorValue = 20.0f;
    float BaseThreatModifier = 1.0f;

    if (SpecTag.MatchesTagExact(Tag_Guardian_Vanguard))
    {
        // Tank spec: highest health/armor/threat in the game, modest damage.
        BaseHealth = 375.0f;          // -> 1500 at L20
        BaseMana = 62.5f;             // "Energy" pool, small - Guardians are not mana-casters
        BaseAttackPower = 12.5f;      // -> 50 at L20
        BaseArmorValue = 60.0f;       // heavily armored
        BaseThreatModifier = 3.5f;    // constant multiplier, not level-scaled
    }
    else if (SpecTag.MatchesTagExact(Tag_Guardian_Berserker))
    {
        // Off-tank/DPS spec: less health/threat than Vanguard, more damage.
        BaseHealth = 300.0f;          // -> 1200 at L20
        BaseMana = 62.5f;
        BaseAttackPower = 20.0f;      // -> 80 at L20
        BaseArmorValue = 45.0f;
        BaseThreatModifier = 1.5f;
    }
    else if (SpecTag.MatchesTagExact(Tag_Wizard_Elementalist))
    {
        // Ranged burst caster: low health/armor, high mana, moderate damage.
        BaseHealth = 187.5f;          // -> 750 at L20
        BaseMana = 250.0f;            // -> 1000 at L20
        BaseAttackPower = 15.0f;      // -> 60 at L20
        BaseArmorValue = 10.0f;
        BaseThreatModifier = 0.6f;
    }
    else if (SpecTag.MatchesTagExact(Tag_Wizard_BattleMage))
    {
        // Melee-range caster/bruiser: more health/armor than Elementalist,
        // highest raw AttackPower of the two implemented classes, less mana.
        BaseHealth = 237.5f;          // -> 950 at L20
        BaseMana = 225.0f;            // -> 900 at L20
        BaseAttackPower = 30.0f;      // -> 120 at L20
        BaseArmorValue = 22.0f;
        BaseThreatModifier = 0.9f;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("InitializeAttributesForLevel: unrecognized spec tag '%s' - using generic defaults"),
            *SpecTag.ToString());
    }

    const float NewMaxHealth = BaseHealth * LevelMultiplier;
    const float NewMaxMana = BaseMana * LevelMultiplier;

    SetMaxHealth(NewMaxHealth);
    SetHealth(NewMaxHealth);           // full heal on (re)initialize - level-up should feel good, not punish you with a missing chunk of a bigger bar
    SetMaxMana(NewMaxMana);
    SetMana(NewMaxMana);
    SetAttackPower(BaseAttackPower * LevelMultiplier);
    SetArmorValue(BaseArmorValue * LevelMultiplier);
    SetThreatModifier(BaseThreatModifier); // threat modifier is a multiplier, not a scaling stat - deliberately NOT multiplied by LevelMultiplier

    UE_LOG(LogTemp, Log, TEXT("Attributes initialized: Level=%d Class=%s Spec=%s | HP=%.0f Mana=%.0f AP=%.0f Armor=%.0f Threat=%.2f"),
        Level, *ClassTag.ToString(), *SpecTag.ToString(), NewMaxHealth, NewMaxMana, GetAttackPower(), GetArmorValue(), GetThreatModifier());
}
