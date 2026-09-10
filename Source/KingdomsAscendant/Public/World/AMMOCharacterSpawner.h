#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "Persistence/MMOPlayerProfile.h"
#include "AMMOCharacterSpawner.generated.h"

class AMMOCharacter;

/**
 * Orchestrates character creation and loading. This is the SOLE place
 * starter gear is granted and initial attributes are set - see
 * MASTER_CODE_AUDIT.md Finding #2 for why AMMOCharacter::BeginPlay()
 * deliberately does not duplicate this.
 *
 * Functions here are plain server-authoritative functions (not RPCs
 * themselves) - they are expected to be invoked from a PlayerController's
 * own Server RPC (e.g. a character-select UI calling
 * "Server_RequestNewCharacter" on its owning PlayerController, which then
 * calls into this spawner). That keeps ownership of the network boundary
 * with the PlayerController, which is standard UE practice, rather than
 * every world actor needing its own client-callable entry points.
 */
UCLASS()
class MYMMO_API AMMOCharacterSpawner : public AInfo
{
    GENERATED_BODY()

public:
    AMMOCharacterSpawner();

    UPROPERTY(EditInstanceOnly, Category = "Spawner|Config")
    TSubclassOf<class AMMOCharacter> GuardianCharacterClass;

    UPROPERTY(EditInstanceOnly, Category = "Spawner|Config")
    TSubclassOf<class AMMOCharacter> WizardCharacterClass;

    UPROPERTY(EditInstanceOnly, Category = "Spawner|Config")
    FVector NewCharacterSpawnLocation = FVector(0, 0, 100);

    UPROPERTY(EditInstanceOnly, Category = "Spawner|Config")
    UDataTable* QuestDataTable = nullptr;

    /** Spawns a brand-new Level 1 character for a player, with starter gear and base attributes. Server-only. */
    UFUNCTION(BlueprintCallable, Category = "Spawner")
    AMMOCharacter* Server_SpawnNewCharacter(APlayerController* PC, FGameplayTag ClassTag, FGameplayTag SpecTag, const FString& PlayerUID);

    /** Spawns a character and restores it from a saved profile (level, XP, Aether, completed quests). Server-only. */
    UFUNCTION(BlueprintCallable, Category = "Spawner")
    AMMOCharacter* Server_LoadCharacter(APlayerController* PC, const FMMOPlayerProfile& Profile);

protected:
    AMMOCharacter* SpawnCharacterOfClass(FGameplayTag ClassTag) const;

    /** New-character setup: Level 1, starter gear, base attributes, fresh quest cache. */
    void SetupNewCharacter(AMMOCharacter* Character, FGameplayTag ClassTag, FGameplayTag SpecTag) const;

    /** Loaded-character setup: restores level/attributes/quest history from a saved profile. */
    void SetupLoadedCharacter(AMMOCharacter* Character, const FMMOPlayerProfile& Profile) const;
};
