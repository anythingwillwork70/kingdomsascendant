#include "Characters/MMOCharacter.h"
#include "AbilitySystem/MMOAbilitySystemComponent.h"
#include "AbilitySystem/MMOAttributeSet.h"
#include "Inventory/MMOInventoryComponent.h"
#include "Quest/MMOQuestComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/KismetSystemLibrary.h"

AMMOCharacter::AMMOCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    bReplicateMovement = true;

    GetCharacterMovement()->MaxWalkSpeed = 600.0f;

    // Create ability system
    AbilitySystemComponent = CreateDefaultSubobject<UMMOAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

    // Create attribute set
    AttributeSet = CreateDefaultSubobject<UMMOAttributeSet>(TEXT("AttributeSet"));

    // Create inventory
    InventoryComponent = CreateDefaultSubobject<UMMOInventoryComponent>(TEXT("InventoryComponent"));

    // Create quest component
    QuestComponent = CreateDefaultSubobject<UMMOQuestComponent>(TEXT("QuestComponent"));
}

void AMMOCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->InitAbilityActorInfo(this, this);
    }

    // NOTE: Starter-gear granting intentionally does NOT happen here.
    // AUDIT FIX: The previous version guarded this block on
    // "!PlayerUID.IsEmpty()", but PlayerUID/ClassTag are set by
    // AMMOCharacterSpawner AFTER spawn (via Server_SetPlayerIdentity on the
    // PlayerState), which runs strictly after BeginPlay. That made this block
    // permanently dead code AND created a second, redundant code path for
    // gear-granting alongside AMMOCharacterSpawner::SetupNewCharacter(), which
    // risked double-granting starter gear if both paths were ever live at once.
    // Starter gear is granted exclusively by AMMOCharacterSpawner. Do not
    // duplicate that call here.
}

void AMMOCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (GetLocalRole() == ROLE_Authority)
    {
        UpdateTargetRotation(DeltaTime);
    }
}

void AMMOCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AMMOCharacter, ClassTag);
    DOREPLIFETIME(AMMOCharacter, SpecTag);
    DOREPLIFETIME(AMMOCharacter, CharacterLevel);
    DOREPLIFETIME(AMMOCharacter, PlayerUID);
    DOREPLIFETIME(AMMOCharacter, CurrentHardTarget);
    DOREPLIFETIME(AMMOCharacter, bIsHardLocked);   // AUDIT FIX: was missing
    DOREPLIFETIME(AMMOCharacter, bIsDead);         // AUDIT FIX: new property
    DOREPLIFETIME(AMMOCharacter, bIsInCombat);     // AUDIT FIX: new property
}

UAbilitySystemComponent* AMMOCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

// ============================================================================
// IMMOTargetableInterface implementation
// ============================================================================

AActor* AMMOCharacter::GetSoftTarget_Implementation() const
{
    return CurrentSoftTarget;
}

AActor* AMMOCharacter::GetHardTarget_Implementation() const
{
    return CurrentHardTarget;
}

bool AMMOCharacter::IsDead_Implementation() const
{
    return bIsDead;
}

bool AMMOCharacter::IsInCombat_Implementation() const
{
    return bIsInCombat;
}

FVector AMMOCharacter::GetTargetLocation_Implementation() const
{
    return GetActorLocation();
}

FVector AMMOCharacter::GetForwardVector_Implementation() const
{
    return GetActorForwardVector();
}

float AMMOCharacter::GetMaxLockRange_Implementation() const
{
    return MaxHardTargetRange;
}

float AMMOCharacter::GetMaxSoftLockRange_Implementation() const
{
    return SoftTargetSearchRadius;
}

void AMMOCharacter::OnHardLockAcquired_Implementation()
{
    // Hook point for VFX/animation/UI on the character being locked onto.
    // Intentionally empty in the vertical slice.
}

void AMMOCharacter::OnHardLockLost_Implementation()
{
    // Hook point for VFX/animation/UI cleanup.
    // Intentionally empty in the vertical slice.
}

FVector AMMOCharacter::GetCameraTargetPoint_Implementation() const
{
    // Aim slightly above the pelvis/root so cameras frame the target's
    // upper body/head rather than their feet.
    return GetActorLocation() + FVector(0.0f, 0.0f, 80.0f);
}

// ============================================================================
// Character state (death / combat)
// ============================================================================

void AMMOCharacter::Server_SetIsDead(bool bNewIsDead)
{
    if (GetLocalRole() != ROLE_Authority) return;

    if (bIsDead == bNewIsDead) return;

    bIsDead = bNewIsDead;
    OnRep_IsDead();

    if (bIsDead)
    {
        // Losing a hard-lock target when it dies is handled reactively by
        // whoever has them locked (their own ValidateHardLockTarget /
        // UpdateTargetRotation checks IsDead_Implementation each tick).
    }
}

void AMMOCharacter::OnRep_IsDead()
{
    // Hook point: ragdoll, disable input, stop montages, etc.
}

void AMMOCharacter::Server_SetIsInCombat(bool bNewIsInCombat)
{
    if (GetLocalRole() != ROLE_Authority) return;
    bIsInCombat = bNewIsInCombat;

    // TODO(FCS Integration): This is the authoritative combat flag other
    // systems (housing build-mode gate, mount-summoning, fast travel, etc.)
    // should query via IsInCombat_Implementation(). Wire your Flexible Combat
    // System's "entered/left combat" events to call this function once FCS
    // is imported into the project. Until that hook exists, nothing sets
    // this to true automatically, so build-mode gating (Fix #2 in the housing
    // bridge) is inert but SAFE - it fails open exactly like the rest of the
    // combat system does pre-FCS-integration, not exploitable.
}

// ============================================================================
// Soft-lock target evaluation (client-only, cosmetic)
// ============================================================================

void AMMOCharacter::EvaluateSoftLockTarget()
{
    if (!IsLocallyControlled()) return;

    FVector ForwardVector = GetActorForwardVector();
    FVector StartLoc = GetActorLocation();

    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

    TArray<AActor*> IgnoredActors;
    IgnoredActors.Add(this);

    TArray<AActor*> OutActors;

    UKismetSystemLibrary::SphereOverlapActors(
        GetWorld(),
        StartLoc,
        SoftTargetSearchRadius,
        ObjectTypes,
        nullptr,
        IgnoredActors,
        OutActors
    );

    AActor* BestTarget = nullptr;
    float HighestScore = -9999.0f;

    for (AActor* Candidate : OutActors)
    {
        // Intentionally NOT checking line-of-sight here (bCheckLineOfSight = false
        // default) - this loop can run over dozens of candidates every time it's
        // called, and a raycast per candidate is not worth paying for a soft,
        // cosmetic target that gets re-evaluated on a short timer.
        if (!IsValidCombatTarget(Candidate)) continue;

        FVector TargetDir = (Candidate->GetActorLocation() - StartLoc).GetSafeNormal();
        float DotProduct = FVector::DotProduct(ForwardVector, TargetDir);
        float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(DotProduct, -1.0f, 1.0f)));

        if (Angle <= (MaxConeAngle / 2.0f))
        {
            float Distance = FVector::Dist(StartLoc, Candidate->GetActorLocation());
            float Score = (DotProduct * 2.0f) - (Distance / SoftTargetSearchRadius);

            if (Candidate == CurrentSoftTarget)
            {
                Score += 0.4f; // stickiness bonus - avoids target flicker
            }

            if (Score > HighestScore)
            {
                HighestScore = Score;
                BestTarget = Candidate;
            }
        }
    }

    CurrentSoftTarget = BestTarget;
}

// ============================================================================
// Hard-lock toggle - CLIENT PREDICTION + SERVER CONFIRMATION
// ============================================================================

void AMMOCharacter::ToggleTargetLockMode()
{
    if (!IsLocallyControlled()) return;

    const bool bWantsToLock = !bIsHardLocked;
    AActor* DesiredTarget = nullptr;

    if (bWantsToLock)
    {
        if (!CurrentSoftTarget || !IsValidCombatTarget(CurrentSoftTarget))
        {
            return; // nothing valid to lock onto - don't even round-trip to server
        }
        DesiredTarget = CurrentSoftTarget;
    }

    // Local prediction: makes the camera/UI feel instant on the owning client.
    // This is redundant-but-harmless on a listen server (where the client IS
    // the server), and is corrected by replication on a dedicated server as
    // soon as Server_ToggleTargetLockMode resolves.
    bIsHardLocked = bWantsToLock;
    CurrentHardTarget = DesiredTarget;

    Server_ToggleTargetLockMode(DesiredTarget);
}

bool AMMOCharacter::Server_ToggleTargetLockMode_Validate(AActor* DesiredHardTarget)
{
    // Deliberately permissive. Gameplay-rule rejection (target too far, dead,
    // out of LOS, etc.) happens in _Implementation and simply no-ops instead
    // of failing here. A failed _Validate causes the engine to close the
    // client's connection - it must never be used for ordinary gameplay
    // rules that can legitimately race against network latency (e.g. target
    // died between click and RPC arrival).
    return true;
}

void AMMOCharacter::Server_ToggleTargetLockMode_Implementation(AActor* DesiredHardTarget)
{
    if (DesiredHardTarget && IsValidCombatTarget(DesiredHardTarget, /*bCheckLineOfSight=*/true))
    {
        bIsHardLocked = true;
        CurrentHardTarget = DesiredHardTarget;

        if (DesiredHardTarget->GetClass()->ImplementsInterface(UMMOTargetableInterface::StaticClass()))
        {
            IMMOTargetableInterface::Execute_OnHardLockAcquired(DesiredHardTarget);
        }
    }
    else
    {
        bIsHardLocked = false;
        CurrentHardTarget = nullptr;
    }
}

// ============================================================================
// Authoritative per-tick target rotation (server only)
// ============================================================================

void AMMOCharacter::UpdateTargetRotation(float DeltaTime)
{
    AActor* TargetToFace = nullptr;

    // Hard-lock is checked WITH line-of-sight every tick, since there is only
    // ever one hard-lock target to validate (cheap), unlike the soft-target
    // candidate scan.
    if (bIsHardLocked && IsValidCombatTarget(CurrentHardTarget, /*bCheckLineOfSight=*/true))
    {
        TargetToFace = CurrentHardTarget;
    }
    else if (bIsHardLocked)
    {
        // Target became invalid (died / out of range / LOS lost) - break the
        // lock authoritatively so it replicates down to everyone.
        bIsHardLocked = false;
        CurrentHardTarget = nullptr;
    }
    else if (CurrentSoftTarget && IsValidCombatTarget(CurrentSoftTarget))
    {
        TargetToFace = CurrentSoftTarget;
    }

    if (TargetToFace)
    {
        FVector TargetDir = (TargetToFace->GetActorLocation() - GetActorLocation()).GetSafeNormal();
        FRotator TargetRot = TargetDir.Rotation();
        TargetRot.Pitch = 0.0f;
        TargetRot.Roll = 0.0f;

        FRotator NewRotation = FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaTime, RotationInterpSpeed);
        SetActorRotation(NewRotation);
    }
}

// ============================================================================
// Target validation
// ============================================================================

bool AMMOCharacter::IsValidCombatTarget(const AActor* TargetCandidate, bool bCheckLineOfSight) const
{
    // AUDIT FIX: use IsValid() rather than a raw null check. A UPROPERTY
    // pointer to a Destroy()'d actor is only nulled by the next garbage
    // collection pass, not instantly - IsValid() additionally checks the
    // pending-kill/pending-destroy flags so a just-destroyed actor is
    // rejected immediately instead of one GC cycle later.
    if (!IsValid(TargetCandidate)) return false;

    // AUDIT FIX: death was never checked here despite the design calling for
    // "hard-lock breaks on target death". Query via the interface rather than
    // casting to AMMOCharacter directly, so non-character actors (e.g. future
    // destructible objects) can also be valid combat targets.
    if (TargetCandidate->GetClass()->ImplementsInterface(UMMOTargetableInterface::StaticClass()))
    {
        if (IMMOTargetableInterface::Execute_IsDead(TargetCandidate))
        {
            return false;
        }
    }

    float Distance = FVector::Dist(GetActorLocation(), TargetCandidate->GetActorLocation());
    if (Distance > MaxHardTargetRange) return false;

    if (bCheckLineOfSight && !RunLineOfSightCheck(const_cast<AActor*>(TargetCandidate)))
    {
        return false;
    }

    return true;
}

bool AMMOCharacter::RunLineOfSightCheck(AActor* TargetActor) const
{
    if (!TargetActor) return false;

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(TargetActor);

    bool bHit = GetWorld()->LineTraceSingleByChannel(
        Hit,
        GetActorLocation() + FVector(0, 0, 90.0f),
        TargetActor->GetActorLocation() + FVector(0, 0, 90.0f),
        ECC_Visibility,
        Params
    );

    return !bHit;
}
