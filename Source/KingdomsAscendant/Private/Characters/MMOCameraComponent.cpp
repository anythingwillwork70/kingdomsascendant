#include "Characters/MMOCameraComponent.h"
#include "Characters/MMOCharacter.h"
#include "Interfaces/MMOTargetableInterface.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

DECLARE_CYCLE_STAT(TEXT("Camera Update"), STAT_MMOCameraUpdate, STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("Camera Validation"), STAT_CameraValidation, STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("Camera Occlusion Check"), STAT_CameraOcclusion, STATGROUP_Game);

// ============================================================================
// CONSTRUCTOR
// ============================================================================

UMMOCameraComponent::UMMOCameraComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f;

    bUsePawnControlRotation = false;
    bUseControllerViewRotation = false;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void UMMOCameraComponent::BeginPlay()
{
    Super::BeginPlay();

    OwnerCharacter = Cast<AMMOCharacter>(GetOwner());
    if (!OwnerCharacter)
    {
        UE_LOG(LogTemp, Error, TEXT("MMOCameraComponent must be attached to AMMOCharacter"));
        SetComponentTickEnabled(false);
        return;
    }

    // ========================================================================
    // AUDIT FIX (MAJOR - PERFORMANCE): the previous version ticked this
    // component's full update logic (line traces for LOS + occlusion,
    // interpolation math) for EVERY AMMOCharacter instance, including every
    // remote/simulated-proxy pawn belonging to OTHER players and every NPC
    // that happened to carry this component. You only ever render your own
    // camera - nobody needs another player's camera computed on your machine,
    // and the server (if the server also spawns this component on characters,
    // e.g. via a shared Blueprint) never needs it at all. At MMO scale
    // (hundreds of concurrent characters per shard) this was doing raycasts
    // every frame for every character in the level, on every client. Ticking
    // is now disabled entirely for any instance that isn't the locally
    // controlled player's own camera, rather than early-returning inside
    // TickComponent every frame (which would still pay scheduler overhead).
    // See MASTER_CODE_AUDIT.md Finding #6.
    // ========================================================================
    if (!OwnerCharacter->IsLocallyControlled())
    {
        SetComponentTickEnabled(false);
        return;
    }

    CameraDesiredLocation = GetComponentLocation();
    CameraDesiredRotation = GetComponentRotation();
}

// ============================================================================
// TICK
// ============================================================================

void UMMOCameraComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    SCOPE_CYCLE_COUNTER(STAT_MMOCameraUpdate);

    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!OwnerCharacter) return;

    TrackFrameTime(DeltaTime);

    switch (CurrentCameraMode)
    {
        case ECameraMode::FreeLook:  UpdateFreeLookCamera(DeltaTime); break;
        case ECameraMode::SoftLock:  UpdateSoftLockCamera(DeltaTime); break;
        case ECameraMode::HardLock:  UpdateHardLockCamera(DeltaTime); break;
        case ECameraMode::BuildMode: UpdateBuildModeCamera(DeltaTime); break;
        default:                     UpdateFreeLookCamera(DeltaTime); break;
    }

    float CurrentLerpSpeed = (CurrentCameraMode == ECameraMode::HardLock) ? HardLockLerpSpeed : CameraLerpSpeed;

    SetWorldLocation(FMath::VInterpTo(GetComponentLocation(), CameraDesiredLocation, DeltaTime, CurrentLerpSpeed), false);
    SetWorldRotation(FMath::RInterpTo(GetComponentRotation(), CameraDesiredRotation, DeltaTime, CurrentLerpSpeed), false);
}

void UMMOCameraComponent::TrackFrameTime(float DeltaTime)
{
    if (DeltaTime > 0.033f && bDebugLogging)
    {
        UE_LOG(LogTemp, Warning, TEXT("Camera frame time stalled: %.2fms (%.1f FPS)"), DeltaTime * 1000.0f, 1.0f / DeltaTime);
    }
    LastFrameUpdateTime = DeltaTime;
}

// ============================================================================
// MODE TRANSITIONS
// ============================================================================

bool UMMOCameraComponent::CanTransitionToMode(ECameraMode FromMode, ECameraMode ToMode) const
{
    static const bool ValidTransitions[4][4] = {
        //         To:   Free  Soft  Hard  Build
        /* Free */ { true, true, true, true  },
        /* Soft */ { true, true, true, false },
        /* Hard */ { true, false,true, false },
        /* Build*/{ true, false,false,true  },
    };

    int32 FromIdx = (int32)FromMode;
    int32 ToIdx = (int32)ToMode;
    if (FromIdx < 0 || FromIdx >= 4 || ToIdx < 0 || ToIdx >= 4) return false;

    return ValidTransitions[FromIdx][ToIdx];
}

void UMMOCameraComponent::SetCameraMode(ECameraMode NewMode)
{
    if (!CanTransitionToMode(CurrentCameraMode, NewMode))
    {
        if (bDebugLogging)
        {
            UE_LOG(LogTemp, Warning, TEXT("Invalid camera mode transition: %d -> %d"), (int32)CurrentCameraMode, (int32)NewMode);
        }
        return;
    }

    PreviousCameraMode = CurrentCameraMode;
    CurrentCameraMode = NewMode;
}

// ============================================================================
// HARD-LOCK TARGET MANAGEMENT
// ============================================================================

void UMMOCameraComponent::SetHardLockTarget(AActor* NewTarget)
{
    if (!NewTarget)
    {
        ClearHardLockTarget();
        return;
    }

    HardLockTarget = NewTarget;
    SetCameraMode(ECameraMode::HardLock);
}

void UMMOCameraComponent::ClearHardLockTarget()
{
    HardLockTarget = nullptr;
    SetCameraMode(ECameraMode::FreeLook);
}

// ============================================================================
// BUILD MODE CAMERA CONTROL
// ============================================================================

void UMMOCameraComponent::SetBuildModeActive(bool bActive)
{
    SetCameraMode(bActive ? ECameraMode::BuildMode : ECameraMode::FreeLook);
    if (bActive)
    {
        BuildModePanOffset = FVector::ZeroVector;
    }
}

void UMMOCameraComponent::PanBuildModeCamera(float DeltaX, float DeltaY)
{
    if (CurrentCameraMode != ECameraMode::BuildMode) return;

    BuildModePanOffset.X = FMath::Clamp(BuildModePanOffset.X + DeltaX * BuildModePanSpeed, -BuildModePanDistance, BuildModePanDistance);
    BuildModePanOffset.Y = FMath::Clamp(BuildModePanOffset.Y + DeltaY * BuildModePanSpeed, -BuildModePanDistance, BuildModePanDistance);
}

// ============================================================================
// FREE-LOOK CAMERA UPDATE
// ============================================================================

void UMMOCameraComponent::UpdateFreeLookCamera(float DeltaTime)
{
    if (!OwnerCharacter) return;

    FVector CharacterLoc = OwnerCharacter->GetActorLocation();
    FVector CameraOffset = OwnerCharacter->GetActorForwardVector() * (-FreeLookDistance) + FVector(0, 0, FreeLookHeight);
    FVector DesiredLoc = CharacterLoc + CameraOffset;

    // AUDIT FIX: occlusion is now checked here too, not just in HardLock -
    // FreeLook is the default mode and was previously the one MOST likely to
    // clip the camera through geometry (walking backward into a wall, etc).
    if (bCheckCameraOcclusion)
    {
        ResolveCameraOcclusion(CharacterLoc, DesiredLoc);
    }

    CameraDesiredLocation = DesiredLoc;

    FVector DirectionToCharacter = (CharacterLoc - CameraDesiredLocation).GetSafeNormal();
    CameraDesiredRotation = DirectionToCharacter.Rotation();
}

// ============================================================================
// SOFT-LOCK CAMERA UPDATE
// ============================================================================

void UMMOCameraComponent::UpdateSoftLockCamera(float DeltaTime)
{
    if (!OwnerCharacter) return;

    // AUDIT FIX: now actually uses IMMOTargetableInterface (previously left as
    // a direct OwnerCharacter->CurrentSoftTarget access with a TODO comment
    // claiming it needed a "future refactor" - the interface existed and was
    // never wired up). AMMOCharacter now implements the interface, so this
    // works as intended and the component no longer depends on
    // AMMOCharacter's concrete layout.
    AActor* SoftTarget = IMMOTargetableInterface::Execute_GetSoftTarget(OwnerCharacter);

    if (!SoftTarget)
    {
        UpdateFreeLookCamera(DeltaTime);
        return;
    }

    FVector CharacterLoc = OwnerCharacter->GetActorLocation();
    FVector TargetLoc = SoftTarget->GetActorLocation();
    FVector Midpoint = (CharacterLoc + TargetLoc) / 2.0f;

    FVector CameraOffset = -OwnerCharacter->GetActorForwardVector() * FreeLookDistance + FVector(0, 0, FreeLookHeight);
    FVector DesiredLoc = Midpoint + CameraOffset;

    if (bCheckCameraOcclusion)
    {
        ResolveCameraOcclusion(Midpoint, DesiredLoc);
    }

    CameraDesiredLocation = DesiredLoc;

    FVector DirectionToMidpoint = (Midpoint - CameraDesiredLocation).GetSafeNormal();
    CameraDesiredRotation = DirectionToMidpoint.Rotation();
}

// ============================================================================
// HARD-LOCK CAMERA UPDATE
// ============================================================================

void UMMOCameraComponent::UpdateHardLockCamera(float DeltaTime)
{
    SCOPE_CYCLE_COUNTER(STAT_CameraValidation);

    if (!ValidateHardLockTarget())
    {
        ClearHardLockTarget();
        return;
    }

    FVector TargetLoc = HardLockTarget->GetActorLocation();
    FVector CharacterLoc = OwnerCharacter->GetActorLocation();

    FVector CameraOffset;
    if (bRotateWithCharacter)
    {
        FVector ToCharacter = (CharacterLoc - TargetLoc).GetSafeNormal();
        CameraOffset = ToCharacter * HardLockDistance + FVector(0, 0, HardLockHeight);
    }
    else
    {
        CameraOffset = -OwnerCharacter->GetActorForwardVector() * HardLockDistance + FVector(0, 0, HardLockHeight);
    }

    FVector DesiredCameraLoc = TargetLoc + CameraOffset;

    if (bCheckCameraOcclusion)
    {
        ResolveCameraOcclusion(TargetLoc, DesiredCameraLoc);
    }

    CameraDesiredLocation = DesiredCameraLoc;

    FVector DirectionToTarget = (TargetLoc - CameraDesiredLocation).GetSafeNormal();
    CameraDesiredRotation = DirectionToTarget.Rotation();
}

// ============================================================================
// BUILD MODE CAMERA UPDATE
// ============================================================================

void UMMOCameraComponent::UpdateBuildModeCamera(float DeltaTime)
{
    if (!OwnerCharacter) return;

    FVector CharacterLoc = OwnerCharacter->GetActorLocation();
    FVector DesiredLoc = CharacterLoc + FVector(300 + BuildModePanOffset.X, 300 + BuildModePanOffset.Y, BuildModeElevation);

    if (bCheckCameraOcclusion)
    {
        ResolveCameraOcclusion(CharacterLoc, DesiredLoc);
    }

    CameraDesiredLocation = DesiredLoc;

    FVector LookDirection = (CharacterLoc - CameraDesiredLocation).GetSafeNormal();
    CameraDesiredRotation = LookDirection.Rotation();
}

// ============================================================================
// TARGET VALIDATION
// ============================================================================

bool UMMOCameraComponent::ValidateHardLockTarget()
{
    SCOPE_CYCLE_COUNTER(STAT_CameraValidation);

    if (!IsValid(HardLockTarget)) return false;

    // AUDIT FIX: death check now goes through the interface instead of a
    // Cast<AMMOCharacter> + direct bIsDead read, so any actor implementing
    // IMMOTargetableInterface (not just AMMOCharacter) can be a valid
    // hard-lock target.
    if (HardLockTarget->GetClass()->ImplementsInterface(UMMOTargetableInterface::StaticClass()))
    {
        if (IMMOTargetableInterface::Execute_IsDead(HardLockTarget))
        {
            return false;
        }
    }

    if (!IsTargetInRange(HardLockTarget))
    {
        return false;
    }

    if (bValidateTargetLOS)
    {
        FVector CharLoc = OwnerCharacter->GetActorLocation();
        FVector TargetLoc = HardLockTarget->GetActorLocation();
        if (!HasLineOfSight(CharLoc, TargetLoc, HardLockTarget))
        {
            return false;
        }
    }

    return true;
}

bool UMMOCameraComponent::IsTargetInRange(AActor* Target) const
{
    if (!Target || !OwnerCharacter) return false;
    return FVector::Dist(OwnerCharacter->GetActorLocation(), Target->GetActorLocation()) <= MaxLockRange;
}

bool UMMOCameraComponent::HasLineOfSight(const FVector& FromLoc, const FVector& ToLoc, AActor* IgnoreExtra) const
{
    if (!GetWorld()) return false;

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwnerCharacter);
    if (IgnoreExtra) Params.AddIgnoredActor(IgnoreExtra);

    bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, FromLoc + FVector(0, 0, 100), ToLoc + FVector(0, 0, 100), ECC_Visibility, Params);

    return !bHit || Hit.GetActor() == IgnoreExtra;
}

// ============================================================================
// SHARED CAMERA OCCLUSION RESOLVER (used by all four modes)
// ============================================================================

bool UMMOCameraComponent::ResolveCameraOcclusion(const FVector& PivotLoc, FVector& InOutCameraLoc)
{
    SCOPE_CYCLE_COUNTER(STAT_CameraOcclusion);

    if (!GetWorld()) return false;

    FHitResult CameraHit;
    FCollisionQueryParams CameraParams;
    CameraParams.AddIgnoredActor(OwnerCharacter);
    if (HardLockTarget) CameraParams.AddIgnoredActor(HardLockTarget);

    bool bObstructed = GetWorld()->LineTraceSingleByChannel(CameraHit, PivotLoc, InOutCameraLoc, ECC_Camera, CameraParams);

    if (bObstructed && CameraHit.bBlockingHit)
    {
        InOutCameraLoc = CameraHit.ImpactPoint + CameraHit.ImpactNormal * CameraObstructionPushBack;
        return true;
    }

    return false;
}
