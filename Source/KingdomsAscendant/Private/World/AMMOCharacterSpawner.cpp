#include "World/AMMOCharacterSpawner.h"
#include "Characters/MMOCharacter.h"
#include "Player/MMOPlayerState.h"
#include "AbilitySystem/MMOAttributeSet.h"
#include "Inventory/MMOInventoryComponent.h"
#include "Quest/MMOQuestComponent.h"
#include "GameFramework/PlayerController.h"

AMMOCharacterSpawner::AMMOCharacterSpawner()
{
    PrimaryActorTick.bCanEverTick = false;
}

AMMOCharacter* AMMOCharacterSpawner::SpawnCharacterOfClass(FGameplayTag ClassTag) const
{
    static const FGameplayTag Tag_Guardian = FGameplayTag::RequestGameplayTag(FName("Class.Guardian"), false);
    static const FGameplayTag Tag_Wizard   = FGameplayTag::RequestGameplayTag(FName("Class.Wizard"), false);

    TSubclassOf<AMMOCharacter> ClassToSpawn = nullptr;

    if (ClassTag.MatchesTag(Tag_Guardian))
    {
        ClassToSpawn = GuardianCharacterClass;
    }
    else if (ClassTag.MatchesTag(Tag_Wizard))
    {
        ClassToSpawn = WizardCharacterClass;
    }

    if (!ClassToSpawn)
    {
        UE_LOG(LogTemp, Error, TEXT("SpawnCharacterOfClass: no character Blueprint class configured for '%s'"), *ClassTag.ToString());
        return nullptr;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    FTransform SpawnTransform(FRotator::ZeroRotator, NewCharacterSpawnLocation);

    return GetWorld()->SpawnActor<AMMOCharacter>(ClassToSpawn, SpawnTransform, SpawnParams);
}

AMMOCharacter* AMMOCharacterSpawner::Server_SpawnNewCharacter(APlayerController* PC, FGameplayTag ClassTag, FGameplayTag SpecTag, const FString& PlayerUID)
{
    if (GetLocalRole() != ROLE_Authority)
    {
        UE_LOG(LogTemp, Error, TEXT("Server_SpawnNewCharacter called on non-authority - ignored"));
        return nullptr;
    }

    if (!PC)
    {
        UE_LOG(LogTemp, Error, TEXT("Server_SpawnNewCharacter: null PlayerController"));
        return nullptr;
    }

    AMMOCharacter* NewCharacter = SpawnCharacterOfClass(ClassTag);
    if (!NewCharacter) return nullptr;

    PC->Possess(NewCharacter);

    if (AMMOPlayerState* PS = PC->GetPlayerState<AMMOPlayerState>())
    {
        // Called directly rather than via RPC - we ARE the server, and
        // Server_SetPlayerIdentity's _Implementation runs immediately when
        // invoked from server-side code (no network round-trip needed).
        PS->Server_SetPlayerIdentity_Implementation(PlayerUID, ClassTag, SpecTag);
    }

    SetupNewCharacter(NewCharacter, ClassTag, SpecTag);

    return NewCharacter;
}

AMMOCharacter* AMMOCharacterSpawner::Server_LoadCharacter(APlayerController* PC, const FMMOPlayerProfile& Profile)
{
    if (GetLocalRole() != ROLE_Authority)
    {
        UE_LOG(LogTemp, Error, TEXT("Server_LoadCharacter called on non-authority - ignored"));
        return nullptr;
    }

    if (!PC)
    {
        UE_LOG(LogTemp, Error, TEXT("Server_LoadCharacter: null PlayerController"));
        return nullptr;
    }

    AMMOCharacter* LoadedCharacter = SpawnCharacterOfClass(Profile.ClassTag);
    if (!LoadedCharacter) return nullptr;

    PC->Possess(LoadedCharacter);

    if (AMMOPlayerState* PS = PC->GetPlayerState<AMMOPlayerState>())
    {
        PS->Server_SetPlayerIdentity_Implementation(Profile.PlayerUID, Profile.ClassTag, Profile.SpecTag);
        PS->PlayerLevel = Profile.CharacterLevel;
        PS->ExperiencePoints = Profile.ExperiencePoints;
        PS->AetherCurrency = Profile.AetherCurrency;
    }

    SetupLoadedCharacter(LoadedCharacter, Profile);

    return LoadedCharacter;
}

void AMMOCharacterSpawner::SetupNewCharacter(AMMOCharacter* Character, FGameplayTag ClassTag, FGameplayTag SpecTag) const
{
    if (!Character) return;

    Character->ClassTag = ClassTag;
    Character->SpecTag = SpecTag;
    Character->CharacterLevel = 1;

    if (Character->InventoryComponent)
    {
        Character->InventoryComponent->GrantStarterGear(ClassTag);
    }

    if (Character->GetAttributeSet())
    {
        Character->GetAttributeSet()->InitializeAttributesForLevel(1, ClassTag, SpecTag);
    }

    if (Character->QuestComponent && QuestDataTable)
    {
        Character->QuestComponent->LoadQuestData(QuestDataTable);
    }

    UE_LOG(LogTemp, Log, TEXT("New character created: Class=%s Spec=%s"), *ClassTag.ToString(), *SpecTag.ToString());
}

void AMMOCharacterSpawner::SetupLoadedCharacter(AMMOCharacter* Character, const FMMOPlayerProfile& Profile) const
{
    if (!Character) return;

    Character->ClassTag = Profile.ClassTag;
    Character->SpecTag = Profile.SpecTag;
    Character->CharacterLevel = Profile.CharacterLevel;
    Character->PlayerUID = Profile.PlayerUID;

    if (Character->GetAttributeSet())
    {
        Character->GetAttributeSet()->InitializeAttributesForLevel(Profile.CharacterLevel, Profile.ClassTag, Profile.SpecTag);
    }

    if (Character->QuestComponent)
    {
        if (QuestDataTable)
        {
            Character->QuestComponent->LoadQuestData(QuestDataTable);
        }

        // AUDIT NOTE (Finding #12): explicit FString -> FName conversion at
        // this load boundary, matching the explicit FName -> FString
        // conversion MMOPersistenceSubsystem performs on save. Deliberate,
        // not a shortcut - see that finding for why the two types differ.
        for (const FString& CompletedIDStr : Profile.CompletedQuestIDs)
        {
            Character->QuestComponent->CompletedQuests.AddUnique(FName(*CompletedIDStr));
        }
    }

    // NOTE: starter gear is intentionally NOT granted here - a loaded
    // character's inventory is expected to be restored separately from the
    // profile/save data (equipment persistence is a further piece of scope
    // not yet built; see MASTER_CODE_AUDIT.md for the project's overall
    // status). Granting starter gear to a loaded character would incorrectly
    // duplicate items for a returning player.

    UE_LOG(LogTemp, Log, TEXT("Character loaded: UID=%s Level=%d CompletedQuests=%d"),
        *Profile.PlayerUID, Profile.CharacterLevel, Profile.CompletedQuestIDs.Num());
}
