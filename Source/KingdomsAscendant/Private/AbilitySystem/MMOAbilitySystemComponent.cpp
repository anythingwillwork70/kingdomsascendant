#include "AbilitySystem/MMOAbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilitySpec.h"

UMMOAbilitySystemComponent::UMMOAbilitySystemComponent()
{
}

void UMMOAbilitySystemComponent::GrantStartingAbilities(FGameplayTag ClassTag, FGameplayTag SpecTag, int32 CharacterLevel)
{
    if (GetOwnerRole() != ROLE_Authority)
    {
        UE_LOG(LogTemp, Warning, TEXT("GrantStartingAbilities called on non-authority - ignored"));
        return;
    }

    for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
    {
        if (!AbilityClass) continue;

        FGameplayAbilitySpec Spec(AbilityClass, CharacterLevel, INDEX_NONE, GetOwner());
        GiveAbility(Spec);
    }

    UE_LOG(LogTemp, Log, TEXT("Granted %d starting abilities for %s / %s at level %d"),
        DefaultAbilities.Num(), *ClassTag.ToString(), *SpecTag.ToString(), CharacterLevel);

    // NOTE: DefaultAbilities is empty by default in the vertical slice - this
    // component grants nothing until class-specific GameplayAbility classes
    // are authored and assigned per-class (e.g. via a per-class Blueprint
    // subclass of this component, or a class->ability-array DataTable read
    // here). Left as an explicit, logged no-op rather than a silent stub.
}
