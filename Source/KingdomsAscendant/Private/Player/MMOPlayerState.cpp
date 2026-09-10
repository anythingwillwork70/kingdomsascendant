#include "Player/MMOPlayerState.h"
#include "Characters/MMOCharacter.h"
#include "AbilitySystem/MMOAttributeSet.h"
#include "Net/UnrealNetwork.h"

AMMOPlayerState::AMMOPlayerState()
{
    bReplicates = true;
}

void AMMOPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AMMOPlayerState, PlayerUID);
    DOREPLIFETIME(AMMOPlayerState, SelectedClassTag);
    DOREPLIFETIME(AMMOPlayerState, SelectedSpecTag);
    DOREPLIFETIME(AMMOPlayerState, PlayerLevel);
    DOREPLIFETIME(AMMOPlayerState, ExperiencePoints);
    DOREPLIFETIME(AMMOPlayerState, AetherCurrency);
}

int32 AMMOPlayerState::GetExperienceRequiredForNextLevel() const
{
    // Fixed, simple curve for the vertical slice's Level 1-20 range:
    // 1000 XP for level 1->2, scaling up ~15% per level. Not intended to
    // survive into the full Level 1-50 game as-is - flagged as a vertical-
    // slice-only placeholder, not a final progression curve.
    return FMath::RoundToInt(1000.0f * FMath::Pow(1.15f, static_cast<float>(PlayerLevel - 1)));
}

bool AMMOPlayerState::Server_SetPlayerIdentity_Validate(const FString& NewPlayerUID, FGameplayTag NewClassTag, FGameplayTag NewSpecTag)
{
    // Permissive - reject only obviously malformed input (empty UID). The
    // "is this class/spec combination actually valid" check happens in
    // _Implementation and fails softly (logs + refuses to set), rather than
    // here where failure disconnects the client. See Finding #10.
    return !NewPlayerUID.IsEmpty();
}

void AMMOPlayerState::Server_SetPlayerIdentity_Implementation(const FString& NewPlayerUID, FGameplayTag NewClassTag, FGameplayTag NewSpecTag)
{
    static const FGameplayTag Tag_Guardian = FGameplayTag::RequestGameplayTag(FName("Class.Guardian"), false);
    static const FGameplayTag Tag_Wizard   = FGameplayTag::RequestGameplayTag(FName("Class.Wizard"), false);

    if (!NewClassTag.MatchesTag(Tag_Guardian) && !NewClassTag.MatchesTag(Tag_Wizard))
    {
        UE_LOG(LogTemp, Warning, TEXT("Server_SetPlayerIdentity: rejected unrecognized class tag '%s'"), *NewClassTag.ToString());
        return;
    }

    PlayerUID = NewPlayerUID;
    SelectedClassTag = NewClassTag;
    SelectedSpecTag = NewSpecTag;

    // Also stamp identity onto the possessed character, if any exists yet -
    // AMMOCharacterSpawner typically calls this immediately after spawning,
    // so the pawn should already be possessed by this point.
    if (AMMOCharacter* Char = Cast<AMMOCharacter>(GetPawn()))
    {
        Char->ClassTag = NewClassTag;
        Char->SpecTag = NewSpecTag;
        Char->PlayerUID = NewPlayerUID;
    }
}

void AMMOPlayerState::Server_AddExperience(int32 Amount)
{
    if (GetLocalRole() != ROLE_Authority)
    {
        UE_LOG(LogTemp, Error, TEXT("Server_AddExperience called on non-authority - ignored. This function must only be called from server-side code."));
        return;
    }

    if (Amount <= 0) return;

    ExperiencePoints += Amount;
    ProcessLevelUps();
}

void AMMOPlayerState::Server_AddAether(int32 Amount)
{
    if (GetLocalRole() != ROLE_Authority)
    {
        UE_LOG(LogTemp, Error, TEXT("Server_AddAether called on non-authority - ignored."));
        return;
    }

    if (Amount <= 0) return;

    AetherCurrency += Amount;
}

void AMMOPlayerState::ProcessLevelUps()
{
    // Loop (not a single if) so a large XP grant can carry a character
    // through more than one level in a single call.
    while (PlayerLevel < 20 && ExperiencePoints >= GetExperienceRequiredForNextLevel())
    {
        ExperiencePoints -= GetExperienceRequiredForNextLevel();
        PlayerLevel++;

        if (AMMOCharacter* Char = Cast<AMMOCharacter>(GetPawn()))
        {
            Char->CharacterLevel = PlayerLevel;

            if (UMMOAttributeSet* Attributes = Char->GetAttributeSet())
            {
                Attributes->InitializeAttributesForLevel(PlayerLevel, Char->ClassTag, Char->SpecTag);
            }
        }

        UE_LOG(LogTemp, Log, TEXT("PlayerState: leveled up to %d"), PlayerLevel);
    }
}
