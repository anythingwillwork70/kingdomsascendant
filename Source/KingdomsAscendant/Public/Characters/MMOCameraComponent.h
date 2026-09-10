#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "InputActionValue.h"
#include "MMOCameraComponent.generated.h"

// ============================================================================
// CAMERA MODE ENUM
// ============================================================================

UENUM(BlueprintType)
enum class ECameraMode : uint8
{
    FreeLook,      // Camera orbits character freely
    SoftLock,      // Camera follows soft-locked target
    HardLock,      // Camera locks to hard-locked target
    BuildMode      // Fixed elevated camera for building
};

// ============================================================================
// CAMERA CONFIGURATION (For DataTable loading)
// ============================================================================

USTRUCT(BlueprintType)
struct FMMOCameraSettings : public FTableRowBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float HardLockDistance = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float HardLockHeight = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float CameraLerpSpeed = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float FreeLookDistance = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float FreeLookHeight = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float MaxLockRange = 1200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float CameraObstructionPushBack = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float BuildModeElevation = 800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float BuildModePanDistance = 300.0f;
};

// ============================================================================
// MAIN CAMERA COMPONENT
// ============================================================================
// AUDIT NOTE: This component reimplements camera-vs-world collision by hand
// (see CheckCameraOcclusion) rather than using USpringArmComponent's built-in
// bDoCollisionTest. That is a deliberate choice here ONLY because hard-lock
// orbiting-a-target-not-the-character behavior does not map onto SpringArm's
// character-relative model. If your team is not comfortable maintaining
// hand-rolled camera collision, consider a hybrid: SpringArmComponent for
// FreeLook/SoftLock/BuildMode (where it orbits the character), and this
// component's logic only for HardLock. See MASTER_CODE_AUDIT.md Finding #9.
// ============================================================================

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MYMMO_API UMMOCameraComponent : public UCameraComponent
{
    GENERATED_BODY()

public:
    UMMOCameraComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================================================
    // CAMERA MODE CONTROL
    // ========================================================================

    UFUNCTION(BlueprintCallable, Category = "Camera")
    void SetCameraMode(ECameraMode NewMode);

    UFUNCTION(BlueprintCallable, Category = "Camera")
    ECameraMode GetCameraMode() const { return CurrentCameraMode; }

    // ========================================================================
    // HARD-LOCK CONTROL
    // ========================================================================

    UFUNCTION(BlueprintCallable, Category = "Camera|HardLock")
    void SetHardLockTarget(AActor* NewTarget);

    UFUNCTION(BlueprintCallable, Category = "Camera|HardLock")
    AActor* GetHardLockTarget() const { return HardLockTarget; }

    UFUNCTION(BlueprintCallable, Category = "Camera|HardLock")
    void ClearHardLockTarget();

    // ========================================================================
    // BUILD MODE CONTROL
    // ========================================================================

    UFUNCTION(BlueprintCallable, Category = "Camera|BuildMode")
    void SetBuildModeActive(bool bActive);

    UFUNCTION(BlueprintCallable, Category = "Camera|BuildMode")
    void PanBuildModeCamera(float DeltaX, float DeltaY);

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    UPROPERTY(EditDefaultsOnly, Category = "Camera|HardLock")
    float HardLockDistance = 400.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Camera|HardLock")
    float HardLockHeight = 100.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Camera|HardLock")
    float MaxLockRange = 1200.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Camera|HardLock")
    bool bRotateWithCharacter = true;

    UPROPERTY(EditDefaultsOnly, Category = "Camera|HardLock")
    bool bValidateTargetLOS = true;

    UPROPERTY(EditDefaultsOnly, Category = "Camera|Occlusion")
    bool bCheckCameraOcclusion = true;

    UPROPERTY(EditDefaultsOnly, Category = "Camera|Lerp")
    float CameraLerpSpeed = 8.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Camera|Lerp")
    float HardLockLerpSpeed = 10.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Camera|FreeLook")
    float FreeLookDistance = 400.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Camera|FreeLook")
    float FreeLookHeight = 100.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Camera|BuildMode")
    float BuildModeElevation = 800.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Camera|BuildMode")
    float BuildModePanDistance = 300.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Camera|BuildMode")
    float BuildModePanSpeed = 5.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Camera|Occlusion")
    float CameraObstructionPushBack = 50.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Camera|Debug")
    bool bDebugLogging = false;

protected:
    ECameraMode CurrentCameraMode = ECameraMode::FreeLook;
    ECameraMode PreviousCameraMode = ECameraMode::FreeLook;

    UPROPERTY()
    AActor* HardLockTarget = nullptr;

    UPROPERTY()
    class AMMOCharacter* OwnerCharacter = nullptr;

    FVector CameraDesiredLocation = FVector::ZeroVector;
    FRotator CameraDesiredRotation = FRotator::ZeroRotator;

    FVector BuildModePanOffset = FVector::ZeroVector;

    double LastFrameUpdateTime = 0.0;

    // ========================================================================
    // CAMERA UPDATE FUNCTIONS
    // ========================================================================

    void UpdateFreeLookCamera(float DeltaTime);
    void UpdateSoftLockCamera(float DeltaTime);
    void UpdateHardLockCamera(float DeltaTime);
    void UpdateBuildModeCamera(float DeltaTime);

    // ========================================================================
    // VALIDATION & UTILITY FUNCTIONS
    // ========================================================================

    bool ValidateHardLockTarget();
    bool IsTargetInRange(AActor* Target) const;
    bool HasLineOfSight(const FVector& FromLoc, const FVector& ToLoc, AActor* IgnoreExtra) const;

    /**
     * Shared occlusion resolver used by ALL camera modes (not just HardLock -
     * see MASTER_CODE_AUDIT.md Finding #8: the previous version only applied
     * occlusion checking to HardLock, leaving the default FreeLook mode, used
     * for the large majority of play time, still able to clip through walls).
     * PivotLoc is the point the camera is looking at/orbiting (target in
     * HardLock, character elsewhere); InOutCameraLoc is adjusted in place.
     */
    bool ResolveCameraOcclusion(const FVector& PivotLoc, FVector& InOutCameraLoc);

    bool CanTransitionToMode(ECameraMode FromMode, ECameraMode ToMode) const;

    void TrackFrameTime(float DeltaTime);
};
