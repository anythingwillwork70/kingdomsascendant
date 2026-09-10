#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "MMOAbilitySystemComponent.generated.h"

/**
 * Thin project-specific subclass of UAbilitySystemComponent.
 *
 * Kept intentionally minimal for the vertical slice: GAS's built-in
 * replication (EGameplayEffectReplicationMode::Minimal, set in
 * AMMOCharacter's constructor) and UMMOAttributeSet's own OnRep_* functions
 * handle attribute sync. This class exists as an explicit extension point
 * (ability-granting-on-level-up, class-specific startup abilities) rather
 * than using the stock UAbilitySystemComponent directly, so those hooks have
 * a natural home without touching AMMOCharacter itself later.
 */
UCLASS()
class MYMMO_API UMMOAbilitySystemComponent : public UAbilitySystemComponent
{
    GENERATED_BODY()

public:
    UMMOAbilitySystemComponent();

    /**
     * Grants the class/spec-appropriate starting ability set. Called by
     * AMMOCharacterSpawner after InitializeAttributesForLevel(), server-only.
     * Intentionally a no-op stub in the vertical slice's two implemented
     * classes beyond logging - wire real GameplayAbility classes here once
     * ability Blueprints/C++ classes exist. Documented as a stub, not hidden
     * as if it were complete.
     */
    UFUNCTION(BlueprintCallable, Category = "Abilities")
    void GrantStartingAbilities(FGameplayTag ClassTag, FGameplayTag SpecTag, int32 CharacterLevel);

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Abilities")
    TArray<TSubclassOf<class UGameplayAbility>> DefaultAbilities;
};
