#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MMOTargetableInterface.generated.h"

// ============================================================================
// TARGETABLE INTERFACE - FIX FOR MAJOR ISSUE #5 (Tight Coupling)
// ============================================================================
// This interface decouples camera and targeting components from AMMOCharacter.
// Instead of direct property access, use these interface methods.
// ============================================================================

UINTERFACE(MinimalAPI, Blueprintable)
class UMMOTargetableInterface : public UInterface
{
    GENERATED_BODY()
};

class IMMOTargetableInterface
{
    GENERATED_BODY()

public:
    // ========================================================================
    // TARGET INFORMATION
    // ========================================================================

    /** Get the current soft-locked target (for camera positioning) */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Targeting")
    AActor* GetSoftTarget() const;

    /** Get the current hard-locked target (for melee attacks) */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Targeting")
    AActor* GetHardTarget() const;

    // ========================================================================
    // CHARACTER STATE
    // ========================================================================

    /** Check if this character is dead */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Character")
    bool IsDead() const;

    /** Check if this character is currently in combat */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Character")
    bool IsInCombat() const;

    /** Get this character's current location */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Character")
    FVector GetTargetLocation() const;

    /** Get this character's forward vector */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Character")
    FVector GetForwardVector() const;

    // ========================================================================
    // TARGETING RANGES & CONSTRAINTS
    // ========================================================================

    /** Maximum distance this character can be hard-locked from (1200u per GDD) */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Targeting")
    float GetMaxLockRange() const;

    /** Maximum distance this character can be soft-locked from */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Targeting")
    float GetMaxSoftLockRange() const;

    // ========================================================================
    // CAMERA INTEGRATION
    // ========================================================================

    /** Called when this actor becomes hard-locked by a camera */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Camera")
    void OnHardLockAcquired();

    /** Called when hard-lock is lost */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Camera")
    void OnHardLockLost();

    /** Get a point slightly above the head for camera targeting */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Camera")
    FVector GetCameraTargetPoint() const;
};
