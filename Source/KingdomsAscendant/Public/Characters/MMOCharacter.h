#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "AbilitySystemInterface.h"
#include "Interfaces/MMOTargetableInterface.h"
#include "MMOCharacter.generated.h"

class UAbilitySystemComponent;
class UMMOAttributeSet;
class UMMOInventoryComponent;
class UMMOQuestComponent;

UCLASS()
class MYMMO_API AMMOCharacter : public ACharacter, public IAbilitySystemInterface, public IMMOTargetableInterface
{
    GENERATED_BODY()

public:
    AMMOCharacter();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // IAbilitySystemInterface
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    // ========================================================================
    // IMMOTargetableInterface - implemented so other systems (camera, housing,
    // combat) can query this character WITHOUT reaching into its properties
    // directly. See AUDIT FIX #5 / #31 in MASTER_CODE_AUDIT.md.
    // ========================================================================
    virtual AActor* GetSoftTarget_Implementation() const override;
    virtual AActor* GetHardTarget_Implementation() const override;
    virtual bool IsDead_Implementation() const override;
    virtual bool IsInCombat_Implementation() const override;
    virtual FVector GetTargetLocation_Implementation() const override;
    virtual FVector GetForwardVector_Implementation() const override;
    virtual float GetMaxLockRange_Implementation() const override;
    virtual float GetMaxSoftLockRange_Implementation() const override;
    virtual void OnHardLockAcquired_Implementation() override;
    virtual void OnHardLockLost_Implementation() override;
    virtual FVector GetCameraTargetPoint_Implementation() const override;

    // --- Character Identity ---
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Character|Identity")
    FGameplayTag ClassTag;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Character|Identity")
    FGameplayTag SpecTag;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Character|Identity")
    int32 CharacterLevel = 1;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Character|Identity")
    FString PlayerUID;

    // --- Character State ---
    // AUDIT FIX: bIsDead and bIsInCombat did not exist despite being referenced
    // by the housing bridge / camera fixes. They are the authoritative source
    // of truth other systems query through IMMOTargetableInterface.
    UPROPERTY(ReplicatedUsing = OnRep_IsDead, BlueprintReadOnly, Category = "Character|State")
    bool bIsDead = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Character|State")
    bool bIsInCombat = false;

    UFUNCTION()
    void OnRep_IsDead();

    /** Server-authoritative setters. Call these from your death/combat systems (FCS hooks, damage pipeline). */
    UFUNCTION(BlueprintCallable, Category = "Character|State")
    void Server_SetIsDead(bool bNewIsDead);

    UFUNCTION(BlueprintCallable, Category = "Character|State")
    void Server_SetIsInCombat(bool bNewIsInCombat);

    // --- Targeting ---
    // NOTE: CurrentSoftTarget is intentionally client-only / non-replicated.
    // It is cosmetic (drives the local camera + reticle) and is re-evaluated
    // independently on every client, so it never needs to cross the network.
    UPROPERTY(BlueprintReadOnly, Category = "Character|Combat")
    AActor* CurrentSoftTarget = nullptr;

    // AUDIT FIX: bIsHardLocked previously was NOT replicated even though
    // CurrentHardTarget was. Both must replicate together or remote clients
    // end up with a target but no lock flag (or vice versa).
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Character|Combat")
    bool bIsHardLocked = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Character|Combat")
    AActor* CurrentHardTarget = nullptr;

    // --- Combat Targeting Functions ---
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void EvaluateSoftLockTarget();

    /**
     * Client-callable entry point (bind to input). Performs local prediction
     * for camera responsiveness, then confirms authoritatively with the
     * server via Server_ToggleTargetLockMode.
     *
     * AUDIT FIX (CRITICAL): The previous version mutated bIsHardLocked /
     * CurrentHardTarget directly from a function gated on IsLocallyControlled().
     * On a dedicated server, IsLocallyControlled() is NEVER true for a remote
     * client's pawn, so the authoritative (server-side) state was never set,
     * the value never replicated to other clients, and server-side logic that
     * reads bIsHardLocked (UpdateTargetRotation, gated on ROLE_Authority) never
     * fired. Hard-lock silently did nothing outside of a listen-server host
     * testing their own character. See MASTER_CODE_AUDIT.md Finding #1.
     */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ToggleTargetLockMode();

    UFUNCTION(Server, Reliable, WithValidation, Category = "Combat")
    void Server_ToggleTargetLockMode(AActor* DesiredHardTarget);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool RunLineOfSightCheck(AActor* TargetActor) const;

    // --- Components ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UAbilitySystemComponent* AbilitySystemComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UMMOAttributeSet* AttributeSet;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UMMOInventoryComponent* InventoryComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UMMOQuestComponent* QuestComponent;

    UMMOAttributeSet* GetAttributeSet() const { return AttributeSet; }

protected:
    // --- Targeting Configuration ---
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Targeting")
    float SoftTargetSearchRadius = 800.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Targeting")
    float MaxConeAngle = 120.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Targeting")
    float MaxHardTargetRange = 1200.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Targeting")
    float RotationInterpSpeed = 12.0f;

    /**
     * @param bCheckLineOfSight  Opt-in only. LOS is a raycast; the soft-target
     * sphere-overlap scan can examine dozens of candidates per call, so LOS is
     * NOT checked for every candidate (too expensive). It IS checked for the
     * single active hard-lock target every tick in UpdateTargetRotation.
     */
    bool IsValidCombatTarget(const AActor* TargetCandidate, bool bCheckLineOfSight = false) const;
    void UpdateTargetRotation(float DeltaTime);
};
