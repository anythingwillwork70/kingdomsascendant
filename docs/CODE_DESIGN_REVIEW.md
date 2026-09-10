# KINGDOMS ASCENDANT: Code Design Review & Improvement Recommendations

**Review Date**: September 10, 2026  
**Components Reviewed**: MMOHousingBridgeComponent, MMOCameraComponent, EBS v10 Integration  
**Status**: Critical issues identified, recommendations provided  
**Impact**: Medium-High (affects core systems)

---

## 🔴 CRITICAL ISSUES (MUST FIX)

### 1. Resource Deduction Is a Stub

**Location**: `MMOHousingBridgeComponent::DeductBuildingCosts()` (Line ~150)

```cpp
bool UMMOHousingBridgeComponent::DeductBuildingCosts(FName PieceID)
{
    // TODO: Implement actual inventory deduction
    return true;  // ❌ ALWAYS SUCCEEDS - PLAYERS GET FREE BUILDINGS
}
```

**Problem**: 
- Function is a placeholder with a `// TODO` comment
- Always returns `true` regardless of actual resources
- **Result**: Players can build infinitely without spending materials
- **Severity**: Game-breaking economy exploit

**Fix**:
```cpp
bool UMMOHousingBridgeComponent::DeductBuildingCosts(FName PieceID)
{
    if (!AvailablePieces.Contains(PieceID) || !OwnerInventory)
    {
        return false;
    }

    const FBuildingPieceDefinition& Piece = AvailablePieces[PieceID];
    const FBuildingPieceCost& Cost = Piece.Cost;

    // Check if player has required resources BEFORE deducting
    int32 WoodCount = OwnerInventory->GetItemCountByID(FName("Resource_Wood"));
    int32 StoneCount = OwnerInventory->GetItemCountByID(FName("Resource_Stone"));
    int32 IronCount = OwnerInventory->GetItemCountByID(FName("Resource_Iron"));

    if (!Cost.CanAfford(WoodCount, StoneCount, IronCount))
    {
        UE_LOG(LogTemp, Warning, TEXT("Insufficient resources: Need Wood=%d Stone=%d Iron=%d"),
            Cost.WoodRequired, Cost.StoneRequired, Cost.IronRequired);
        return false;
    }

    // Deduct resources (must happen server-side)
    if (GetOwnerRole() == ROLE_Authority)
    {
        OwnerInventory->RemoveItemByID(FName("Resource_Wood"), Cost.WoodRequired);
        OwnerInventory->RemoveItemByID(FName("Resource_Stone"), Cost.StoneRequired);
        OwnerInventory->RemoveItemByID(FName("Resource_Iron"), Cost.IronRequired);
        
        UE_LOG(LogTemp, Log, TEXT("Building costs deducted successfully"));
        return true;
    }

    return false;
}
```

**Test**: Before shipping, verify that placing 10 structures depletes inventory completely.

---

### 2. Build Mode Doesn't Validate FCS State

**Location**: `MMOHousingBridgeComponent::OnBuildModeActivated()` (Line ~85)

**Problem**:
- No check that Flexible Combat System is actually disabled
- Player could still cast spells while building
- No validation of current character state (can't build while stunned, dead, etc.)
- Movement speed change happens but combat state persists

**Scenario**:
1. Player presses Build Mode while in combat
2. Build mode activates, movement slows to 300 UPS
3. FCS is still active - player can still cast abilities while placing buildings
4. Griefing/exploit potential in PvP zones

**Fix**:
```cpp
void UMMOHousingBridgeComponent::OnBuildModeActivated()
{
    if (GetOwnerRole() != ROLE_Authority) return;

    // ✅ Validate character state
    if (!OwnerCharacter || OwnerCharacter->bIsDead)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot enter build mode: Character is dead"));
        return;
    }

    // ✅ Disable FCS combat state
    if (OwnerCharacter->GetFlexibleCombatSystem())
    {
        OwnerCharacter->GetFlexibleCombatSystem()->DisableCombat();
        UE_LOG(LogTemp, Log, TEXT("FCS Combat disabled for build mode"));
    }

    // ✅ Save previous speed before changing it
    float PreviousSpeed = OwnerCharacter->GetCharacterMovement()->MaxWalkSpeed;
    OwnerCharacter->GetCharacterMovement()->MaxWalkSpeed = 300.0f;

    // ✅ Store for restoration
    StoredCombatSpeed = PreviousSpeed;

    bIsInBuildMode = true;
    OnBuildModeToggled.Broadcast(true);
}

void UMMOHousingBridgeComponent::ExitBuildMode()
{
    if (GetOwnerRole() != ROLE_Authority) return;

    // ✅ Re-enable FCS
    if (OwnerCharacter && OwnerCharacter->GetFlexibleCombatSystem())
    {
        OwnerCharacter->GetFlexibleCombatSystem()->EnableCombat();
    }

    // ✅ Restore previous speed
    if (OwnerCharacter && OwnerCharacter->GetCharacterMovement())
    {
        OwnerCharacter->GetCharacterMovement()->MaxWalkSpeed = StoredCombatSpeed;
    }

    bIsInBuildMode = false;
    OnBuildModeToggled.Broadcast(false);
}
```

**Add to header**:
```cpp
UPROPERTY()
float StoredCombatSpeed = 600.0f;
```

---

### 3. Placement Validation Allows Overlapping Structures

**Location**: `MMOHousingBridgeComponent::ValidatePlacement()` (Line ~130)

**Problem**:
```cpp
bool UMMOHousingBridgeComponent::ValidatePlacement(FName PieceID, const FTransform& PlacementTransform)
{
    // ✅ Checks plot boundary
    // ✅ Checks ground collision
    // ❌ MISSING: Overlapping structure check
    // ❌ MISSING: Minimum distance between structures
}
```

**Result**: Players can stack walls infinitely, creating invisible barriers.

**Fix**:
```cpp
bool UMMOHousingBridgeComponent::ValidatePlacement(FName PieceID, const FTransform& PlacementTransform)
{
    if (!OwnerCharacter) return false;

    FVector OwnerLocation = OwnerCharacter->GetActorLocation();
    FVector PlacementLocation = PlacementTransform.GetLocation();
    float Distance = FVector::Dist(OwnerLocation, PlacementLocation);

    // ✅ Check plot boundary (1000u from owner)
    if (Distance > 1000.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("Placement outside plot boundary"));
        return false;
    }

    // ✅ Check ground collision
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwnerCharacter);

    bool bHit = GetWorld()->LineTraceSingleByChannel(
        Hit,
        PlacementLocation + FVector(0, 0, 200),
        PlacementLocation - FVector(0, 0, 500),
        ECC_WorldStatic,
        Params
    );

    if (!bHit)
    {
        UE_LOG(LogTemp, Warning, TEXT("Placement not on ground"));
        return false;
    }

    // ✅ Check for overlapping structures (NEW)
    FVector PieceSize = FVector(100, 100, 200); // Approximate building piece size
    FCollisionShape CollisionBox = FCollisionShape::MakeBox(PieceSize / 2.0f);

    FCollisionQueryParams OverlapParams;
    OverlapParams.AddIgnoredActor(OwnerCharacter);

    TArray<FOverlapResult> Overlaps;
    GetWorld()->OverlapMultiByChannel(
        Overlaps,
        PlacementLocation,
        FQuat::Identity,
        ECC_Pawn, // Check against structure channel
        CollisionBox,
        OverlapParams
    );

    if (Overlaps.Num() > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Placement overlaps existing structures (%d)"), Overlaps.Num());
        return false;
    }

    // ✅ Check minimum distance (50 units between structures)
    for (const FOverlapResult& Overlap : Overlaps)
    {
        if (Overlap.GetActor())
        {
            float DistToStructure = FVector::Dist(PlacementLocation, Overlap.GetActor()->GetActorLocation());
            if (DistToStructure < 50.0f)
            {
                UE_LOG(LogTemp, Warning, TEXT("Too close to existing structure (%.1f units)"), DistToStructure);
                return false;
            }
        }
    }

    return true;
}
```

---

## 🟡 MAJOR DESIGN ISSUES (SHOULD FIX)

### 4. Camera Component Doesn't Handle Target Loss

**Location**: `MMOCameraComponent::UpdateHardLockCamera()` (Line ~90)

**Current Code**:
```cpp
void UMMOCameraComponent::UpdateHardLockCamera(float DeltaTime)
{
    if (!HardLockTarget)
    {
        ClearHardLockTarget();  // ✅ Checks null
        return;
    }

    // ❌ MISSING:
    // - What if target is out of range (>1200u)?
    // - What if target lost LOS?
    // - What if target died?
}
```

**Problem**: Hard-lock persists even when target is invalid (dead, out of range, behind wall).

**Fix**:
```cpp
void UMMOCameraComponent::UpdateHardLockCamera(float DeltaTime)
{
    // ✅ Validate target still exists
    if (!HardLockTarget)
    {
        ClearHardLockTarget();
        return;
    }

    // ✅ Check if target is dead
    AMMOCharacter* TargetChar = Cast<AMMOCharacter>(HardLockTarget);
    if (TargetChar && TargetChar->bIsDead)
    {
        UE_LOG(LogTemp, Log, TEXT("Hard-lock target died, switching to free-look"));
        ClearHardLockTarget();
        return;
    }

    // ✅ Check distance (1200u max lock range per GDD)
    FVector CharLoc = OwnerCharacter->GetActorLocation();
    FVector TargetLoc = HardLockTarget->GetActorLocation();
    float Distance = FVector::Dist(CharLoc, TargetLoc);

    if (Distance > 1200.0f)
    {
        UE_LOG(LogTemp, Log, TEXT("Hard-lock target out of range (%.1f > 1200)"), Distance);
        ClearHardLockTarget();
        return;
    }

    // ✅ Check line of sight
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwnerCharacter);
    Params.AddIgnoredActor(HardLockTarget);

    bool bHit = GetWorld()->LineTraceSingleByChannel(
        Hit,
        CharLoc + FVector(0, 0, 100),
        TargetLoc + FVector(0, 0, 100),
        ECC_Visibility,
        Params
    );

    if (bHit && Hit.GetActor() != HardLockTarget)
    {
        UE_LOG(LogTemp, Log, TEXT("Hard-lock target lost LOS, switching to free-look"));
        ClearHardLockTarget();
        return;
    }

    // ✅ Rest of hard-lock camera logic...
    FVector CameraOffset = -OwnerCharacter->GetActorForwardVector() * HardLockDistance + FVector(0, 0, HardLockHeight);
    CameraDesiredLocation = TargetLoc + CameraOffset;

    FVector DirectionToTarget = (TargetLoc - CameraDesiredLocation).GetSafeNormal();
    CameraDesiredRotation = DirectionToTarget.Rotation();
}
```

---

### 5. Tight Coupling Between Components

**Problem**:

In `MMOCameraComponent::UpdateSoftLockCamera()`:
```cpp
AActor* SoftTarget = OwnerCharacter->CurrentSoftTarget;  // ❌ Direct property access
```

In `MMOHousingBridgeComponent::BeginPlay()`:
```cpp
OwnerCharacter = Cast<AMMOCharacter>(GetOwner());
OwnerInventory = OwnerCharacter->InventoryComponent;  // ❌ Direct property access
```

**Issue**: Both components directly access AMMOCharacter properties. If AMMOCharacter changes:
- No compile-time warnings
- Runtime crashes possible
- Hard to test components in isolation
- Design is fragile

**Better Design** - Use component interfaces:

```cpp
// Create new interface: MMOTargetableInterface.h
UINTERFACE(MinimalAPI, Blueprintable)
class UMMOTargetableInterface : public UInterface
{
    GENERATED_BODY()
};

class IMMOTargetableInterface
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Targeting")
    AActor* GetSoftTarget() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Targeting")
    bool IsDead() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Targeting")
    float GetMaxLockRange() const;
};
```

**Then in components**:
```cpp
// MMOCameraComponent.cpp
void UMMOCameraComponent::UpdateSoftLockCamera(float DeltaTime)
{
    if (!OwnerCharacter) return;

    // ✅ Use interface instead of direct property
    if (IMMOTargetableInterface::Execute_GetSoftTarget(OwnerCharacter) == nullptr)
    {
        UpdateFreeLookCamera(DeltaTime);
        return;
    }

    AActor* SoftTarget = IMMOTargetableInterface::Execute_GetSoftTarget(OwnerCharacter);
    // ... rest of logic
}
```

**Benefit**: Components are loosely coupled and testable.

---

### 6. Camera Occlusion Not Handled

**Problem**: Camera can clip through walls, terrain, buildings.

**Current**: No collision detection between camera and world.

**Fix**:
```cpp
void UMMOCameraComponent::UpdateHardLockCamera(float DeltaTime)
{
    // ... validation code ...

    FVector TargetLoc = HardLockTarget->GetActorLocation();
    FVector CameraOffset = -OwnerCharacter->GetActorForwardVector() * HardLockDistance + FVector(0, 0, HardLockHeight);
    FVector DesiredCameraLoc = TargetLoc + CameraOffset;

    // ✅ Check for camera obstruction
    FHitResult CameraHit;
    FCollisionQueryParams CameraParams;
    CameraParams.AddIgnoredActor(OwnerCharacter);
    CameraParams.AddIgnoredActor(HardLockTarget);

    bool bCameraObstructed = GetWorld()->LineTraceSingleByChannel(
        CameraHit,
        TargetLoc,
        DesiredCameraLoc,
        ECC_WorldStatic,
        CameraParams
    );

    if (bCameraObstructed && CameraHit.bBlockingHit)
    {
        // ✅ Move camera closer to target to avoid obstruction
        DesiredCameraLoc = CameraHit.ImpactPoint + CameraHit.ImpactNormal * 50.0f;
        UE_LOG(LogTemp, Log, TEXT("Camera adjusted for obstruction at %.1f units"), CameraHit.Distance);
    }

    CameraDesiredLocation = DesiredCameraLoc;
}
```

---

### 7. Hardcoded Values Scattered Throughout

**Current State**:
```cpp
HardLockDistance = 400.0f;        // Hardcoded
HardLockHeight = 100.0f;          // Hardcoded
CameraLerpSpeed = 8.0f;           // Hardcoded
FreeLookDistance = 400.0f;        // Hardcoded
const int32 MaxBuildDistance = 1000;  // Hardcoded
const int32 MaxLockRange = 1200;      // Hardcoded
```

**Problem**: Can't tune without recompiling. Different designers need different settings for testing.

**Better Design** - Create Configuration DataTable:

```cpp
// Create file: Public/Config/MMOCameraConfig.h

USTRUCT(BlueprintType)
struct FMMOCameraSettings : public FTableRowBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HardLockDistance = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HardLockHeight = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CameraLerpSpeed = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FreeLookDistance = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FreeLookHeight = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxLockRange = 1200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinDistanceBetweenStructures = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BuildModeLoweredSpeed = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float NormalCombatSpeed = 600.0f;
};
```

**Usage**:
```cpp
// In MMOCameraComponent.cpp BeginPlay()
if (UMMOGameInstance* GI = Cast<UMMOGameInstance>(GetGameInstance()))
{
    if (UDataTable* CameraConfigDT = GI->GetCameraConfigDataTable())
    {
        FMMOCameraSettings* Config = (FMMOCameraSettings*)CameraConfigDT->FindRow<FMMOCameraSettings>(
            FName("Default"), TEXT("CameraConfig"));

        if (Config)
        {
            HardLockDistance = Config->HardLockDistance;
            HardLockHeight = Config->HardLockHeight;
            CameraLerpSpeed = Config->CameraLerpSpeed;
        }
    }
}
```

**Benefit**: Designers can tweak settings in editor without recompiling.

---

### 8. No Error Feedback to Client

**Problem**: Placement fails silently.

**Current**:
```cpp
if (!ValidatePlacement(PieceID, PlacementTransform))
{
    return;  // ❌ No feedback to player
}
```

**Result**: Player clicks to place building, nothing happens, no explanation why.

**Fix**:
```cpp
UENUM(BlueprintType)
enum class EPlacementFailureReason : uint8
{
    Success = 0,
    OutOfBounds,
    NoGround,
    OverlappingStructure,
    TooClose,
    InsufficientResources,
    InvalidPiece
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_One(FOnPlacementFailed, EPlacementFailureReason, Reason);

// In header:
UPROPERTY(BlueprintAssignable, Category = "Housing|Events")
FOnPlacementFailed OnPlacementFailed;

// In implementation:
bool UMMOHousingBridgeComponent::Server_RequestStructurePlacement_Validate(...)
{
    return true;  // Always validate in Implementation
}

void UMMOHousingBridgeComponent::Server_RequestStructurePlacement_Implementation(...)
{
    EPlacementFailureReason FailureReason = EPlacementFailureReason::Success;

    if (!ValidatePlacement(PieceID, PlacementTransform))
    {
        FailureReason = EPlacementFailureReason::OutOfBounds;  // Or specific reason
    }
    else if (!DeductBuildingCosts(PieceID))
    {
        FailureReason = EPlacementFailureReason::InsufficientResources;
    }
    else
    {
        // Success
        HousingSubsystem->SpawnStructure(PieceID, PlacementTransform, OwnerCharacter->PlayerUID);
        OnStructurePlaced.Broadcast(PieceID, PlacementTransform);
        return;
    }

    OnPlacementFailed.Broadcast(FailureReason);
    UE_LOG(LogTemp, Warning, TEXT("Placement failed: %d"), (int32)FailureReason);
}
```

**UI Usage**:
```cpp
FString FailureMessages[] = {
    TEXT("Placement successful"),
    TEXT("Outside building plot"),
    TEXT("No ground to build on"),
    TEXT("Overlapping existing structure"),
    TEXT("Too close to another structure"),
    TEXT("Insufficient resources"),
    TEXT("Invalid building piece")
};

HousingBridge->OnPlacementFailed.AddDynamic(this, &AHUD::OnPlacementFailed);

void AHUD::OnPlacementFailed(EPlacementFailureReason Reason)
{
    ShowWarning(FailureMessages[(int32)Reason]);
}
```

---

## 🟢 MINOR SUGGESTIONS (NICE TO HAVE)

### 9. Add State Validation to Camera Mode Transitions

```cpp
bool UMMOCameraComponent::CanTransitionToMode(ECameraMode FromMode, ECameraMode ToMode)
{
    // Define valid transitions
    static const bool ValidTransitions[4][4] = {
        // From   To:   Free  Soft  Hard  Build
        /* Free */ { true, true, true, true  },
        /* Soft */ { true, true, true, true  },
        /* Hard */ { true, true, true, true  },
        /* Build*/{ true, false,false,true  },  // Can't transition from Build to Soft/Hard
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
        UE_LOG(LogTemp, Warning, TEXT("Invalid camera mode transition: %d -> %d"),
            (int32)CurrentCameraMode, (int32)NewMode);
        return;
    }

    CurrentCameraMode = NewMode;
}
```

---

### 10. Add Async Resource Deduction

**Current**: Resources deducted synchronously in placement function.

**Better**: Deduct resources asynchronously after visual confirmation.

```cpp
void UMMOHousingBridgeComponent::Server_RequestStructurePlacement_Implementation(...)
{
    // ... validation ...

    if (HousingSubsystem)
    {
        HousingSubsystem->SpawnStructure(PieceID, PlacementTransform, OwnerCharacter->PlayerUID);
        
        // ✅ Delay resource deduction slightly so player sees building first
        GetWorld()->GetTimerManager().SetTimerForNextTick([this, PieceID]()
        {
            DeductBuildingCosts(PieceID);
        });

        OnStructurePlaced.Broadcast(PieceID, PlacementTransform);
    }
}
```

---

### 11. Add Performance Monitoring

```cpp
void UMMOCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    SCOPE_CYCLE_COUNTER(STAT_MMOCameraUpdate);  // For profiling

    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!OwnerCharacter) return;

    // ✅ Log if frame time is too high
    if (DeltaTime > 0.033f)  // >30ms
    {
        UE_LOG(LogTemp, Warning, TEXT("Camera update stalled: %.1fms"), DeltaTime * 1000.0f);
    }

    // ... rest of logic
}
```

---

## 📋 SUMMARY TABLE

| Issue | Severity | Type | Fix Time | Impact |
|-------|----------|------|----------|--------|
| Resource Deduction Stub | 🔴 CRITICAL | Logic | 1 hour | Game-breaking |
| FCS State Not Validated | 🔴 CRITICAL | Logic | 30 min | Exploit/Grief |
| Overlapping Structures | 🔴 CRITICAL | Validation | 1 hour | Griefing |
| Target Loss Detection | 🟡 MAJOR | Logic | 45 min | UX/Bugs |
| Tight Coupling | 🟡 MAJOR | Design | 2 hours | Maintainability |
| Camera Occlusion | 🟡 MAJOR | Logic | 1 hour | UX |
| Hardcoded Values | 🟡 MAJOR | Config | 2 hours | Flexibility |
| No Error Feedback | 🟡 MAJOR | UX | 1.5 hours | Player Confusion |
| State Transitions | 🟢 MINOR | Design | 30 min | Safety |
| Async Deduction | 🟢 MINOR | UX | 30 min | Polish |
| Performance Monitoring | 🟢 MINOR | Perf | 30 min | Optimization |

---

## ✅ RECOMMENDATIONS (PRIORITY ORDER)

### Phase 1: Critical Fixes (Week 1)
1. ✅ Implement `DeductBuildingCosts()` properly
2. ✅ Add FCS state validation to build mode
3. ✅ Add structure overlap checking
4. ✅ Add target loss detection to hard-lock camera

### Phase 2: Design Improvements (Week 2)
5. ✅ Decouple components via interfaces
6. ✅ Add camera occlusion handling
7. ✅ Move hardcoded values to DataTable
8. ✅ Add error feedback system

### Phase 3: Polish (Week 3)
9. ✅ Add state transition validation
10. ✅ Implement async resource deduction
11. ✅ Add performance monitoring

---

## 🚀 REVISED TIMELINE

**Original**: 1 week, 20 hours  
**With Fixes**: 2-3 weeks, 40-50 hours

| Phase | Duration | Tasks | Blocker |
|-------|----------|-------|---------|
| Critical | 1 week | Items 1-4 | YES |
| Design | 1 week | Items 5-8 | NO |
| Polish | 1 week | Items 9-11 | NO |

---

## 🎯 NEXT STEPS

1. Review and approve critical fixes (Phase 1)
2. Decide on interface decoupling (Phase 2)
3. Create DataTable configs (Phase 2)
4. Test with 10+ concurrent players before deployment

**Status**: Code is 60% ready. Critical issues must be fixed before shipping.

