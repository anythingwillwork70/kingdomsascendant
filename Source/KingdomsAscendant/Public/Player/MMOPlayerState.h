#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagContainer.h"
#include "MMOPlayerState.generated.h"

UCLASS()
class MYMMO_API AMMOPlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    AMMOPlayerState();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Identity")
    FString PlayerUID;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Identity")
    FGameplayTag SelectedClassTag;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Identity")
    FGameplayTag SelectedSpecTag;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Identity")
    int32 PlayerLevel = 1;

    // ========================================================================
    // Live runtime progression. Distinct from FMMOPlayerProfile (the saved
    // snapshot written to disk by MMOPersistenceSubsystem) - this is the
    // replicated, "currently playing" copy the HUD/UI reads directly.
    // AMMOCharacterSpawner copies profile -> these fields on load, and
    // MMOPersistenceSubsystem copies these fields -> profile on save.
    // ========================================================================

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Progression")
    int32 ExperiencePoints = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Progression")
    int32 AetherCurrency = 0;

    /** Experience required to reach the next level, from PlayerLevel. Simple fixed curve for the vertical slice (Level 1-20). */
    UFUNCTION(BlueprintCallable, Category = "Progression")
    int32 GetExperienceRequiredForNextLevel() const;

    /**
     * Server-authoritative identity assignment, called once by
     * AMMOCharacterSpawner immediately after spawning a character (new or
     * loaded). This IS a real Server RPC (not a same-process convention
     * function like Server_AddExperience below) because character creation
     * flows are initiated by the client's character-select UI.
     */
    UFUNCTION(Server, Reliable, WithValidation, Category = "Identity")
    void Server_SetPlayerIdentity(const FString& NewPlayerUID, FGameplayTag NewClassTag, FGameplayTag NewSpecTag);

    /**
     * NOTE: These are NOT network RPCs (no UFUNCTION(Server,...) macro).
     * They are plain authoritative functions, named with the Server_ prefix
     * purely as a convention meaning "only ever call this from code that is
     * already running on the server" (e.g. from inside
     * UMMOQuestComponent::Server_NotifyGameplayEvent_Implementation, which is
     * already server-side). Making these real RPCs would be redundant -
     * they're never called directly from client input, only from other
     * server-side authoritative code - and would add an unnecessary
     * network-function indirection for a same-process call.
     */
    UFUNCTION(BlueprintCallable, Category = "Progression")
    void Server_AddExperience(int32 Amount);

    UFUNCTION(BlueprintCallable, Category = "Progression")
    void Server_AddAether(int32 Amount);

protected:
    /** Handles level-up when ExperiencePoints crosses the threshold. May level up more than once per call (e.g. a large quest reward). Server-only. */
    void ProcessLevelUps();
};
