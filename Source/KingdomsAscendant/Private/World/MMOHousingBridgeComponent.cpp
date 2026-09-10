#include "World/MMOHousingBridgeComponent.h"
#include "Characters/MMOCharacter.h"
#include "Interfaces/MMOTargetableInterface.h"
#include "Inventory/MMOInventoryComponent.h"
#include "World/MMOHousingSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"

// ============================================================================
// CONSTRUCTOR
// ============================================================================

UMMOHousingBridgeComponent::UMMOHousingBridgeComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicated(true);
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void UMMOHousingBridgeComponent::BeginPlay()
{
    Super::BeginPlay();

    OwnerCharacter = Cast<AMMOCharacter>(GetOwner());
    if (OwnerCharacter)
    {
        OwnerInventory = OwnerCharacter->InventoryComponent;
        HousingSubsystem = GetWorld()->GetSubsystem<UMMOHousingSubsystem>();
        StoredCombatSpeed = OwnerCharacter->GetCharacterMovement() ?
            OwnerCharacter->GetCharacterMovement()->MaxWalkSpeed : 600.0f;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("MMOHousingBridgeComponent must be attached to AMMOCharacter"));
        return;
    }

    if (AvailablePieces.Num() == 0)
    {
        InitializeDefaultPieces();
    }
}

void UMMOHousingBridgeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UMMOHousingBridgeComponent, bIsInBuildMode);
}

// AUDIT FIX: this function is now declared in the header (it was previously
// called from BeginPlay() without a matching declaration - a compile error).
void UMMOHousingBridgeComponent::InitializeDefaultPieces()
{
    FBuildingPieceDefinition WallPiece;
    WallPiece.PieceID = FName("Wall_Basic");
    WallPiece.EBSMeshPath = TEXT("/Content/Architecture/Ivanov/Walls/ST_Wall_Stone_01");
    WallPiece.Cost.StoneRequired = 10;
    WallPiece.PieceSize = FVector(100, 100, 200);
    AvailablePieces.Add(FName("Wall_Basic"), WallPiece);

    FBuildingPieceDefinition FloorPiece;
    FloorPiece.PieceID = FName("Floor_Basic");
    FloorPiece.EBSMeshPath = TEXT("/Content/Architecture/Polyart/Floors/SM_Floor_Wood_01");
    FloorPiece.Cost.WoodRequired = 5;
    FloorPiece.PieceSize = FVector(100, 100, 50);
    AvailablePieces.Add(FName("Floor_Basic"), FloorPiece);

    FBuildingPieceDefinition RoofPiece;
    RoofPiece.PieceID = FName("Roof_Basic");
    RoofPiece.EBSMeshPath = TEXT("/Content/Architecture/Polyart/Roofs/SM_Roof_Thatch_01");
    RoofPiece.Cost.WoodRequired = 8;
    RoofPiece.PieceSize = FVector(100, 100, 100);
    AvailablePieces.Add(FName("Roof_Basic"), RoofPiece);

    FBuildingPieceDefinition DoorPiece;
    DoorPiece.PieceID = FName("Door_Basic");
    DoorPiece.EBSMeshPath = TEXT("/Content/Architecture/Ivanov/Doors/SM_Door_Stone_01");
    DoorPiece.Cost.WoodRequired = 3;
    DoorPiece.Cost.StoneRequired = 5;
    DoorPiece.PieceSize = FVector(50, 100, 200);
    AvailablePieces.Add(FName("Door_Basic"), DoorPiece);

    UE_LOG(LogTemp, Log, TEXT("Initialized %d building pieces"), AvailablePieces.Num());
}

// ============================================================================
// BUILD MODE TOGGLE - CLIENT ENTRY POINT + SERVER RPC
// ============================================================================

void UMMOHousingBridgeComponent::ToggleBuildMode()
{
    if (!OwnerCharacter || !OwnerCharacter->IsLocallyControlled()) return;

    Server_SetBuildMode(!bIsInBuildMode);
}

bool UMMOHousingBridgeComponent::Server_SetBuildMode_Validate(bool bNewBuildMode)
{
    // Kept permissive - see note on Server_RequestStructurePlacement_Validate
    // below for why gameplay rules must not live in _Validate.
    return true;
}

void UMMOHousingBridgeComponent::Server_SetBuildMode_Implementation(bool bNewBuildMode)
{
    if (bNewBuildMode)
    {
        OnBuildModeActivated();
    }
    else
    {
        ExitBuildMode();
    }
}

// ============================================================================
// BUILD MODE ACTIVATION - validates character state (death / combat)
// ============================================================================

void UMMOHousingBridgeComponent::OnBuildModeActivated()
{
    if (GetOwnerRole() != ROLE_Authority) return;

    EPlacementFailureReason ValidationReason = EPlacementFailureReason::Success;
    if (!ValidateCharacterState(ValidationReason))
    {
        OnPlacementFailed.Broadcast(ValidationReason);
        UE_LOG(LogTemp, Warning, TEXT("Cannot enter build mode: %d"), (int32)ValidationReason);
        return;
    }

    if (OwnerCharacter && OwnerCharacter->GetCharacterMovement())
    {
        StoredCombatSpeed = OwnerCharacter->GetCharacterMovement()->MaxWalkSpeed;
        OwnerCharacter->GetCharacterMovement()->MaxWalkSpeed = BuildModeLoweredSpeed;
    }

    bIsInBuildMode = true;
    OnBuildModeToggled.Broadcast(true);
}

void UMMOHousingBridgeComponent::ExitBuildMode()
{
    if (GetOwnerRole() != ROLE_Authority) return;

    if (OwnerCharacter && OwnerCharacter->GetCharacterMovement())
    {
        OwnerCharacter->GetCharacterMovement()->MaxWalkSpeed = StoredCombatSpeed;
    }

    bIsInBuildMode = false;
    OnBuildModeToggled.Broadcast(false);
}

// ============================================================================
// CHARACTER STATE VALIDATION
// ============================================================================

bool UMMOHousingBridgeComponent::ValidateCharacterState(EPlacementFailureReason& OutReason)
{
    if (!OwnerCharacter)
    {
        OutReason = EPlacementFailureReason::CharacterDead;
        return false;
    }

    // AUDIT FIX: query through the interface (now actually implemented by
    // AMMOCharacter) instead of a nonexistent bIsDead/IsInCombat() direct
    // access, which would not have compiled against the real class.
    if (IMMOTargetableInterface::Execute_IsDead(OwnerCharacter))
    {
        OutReason = EPlacementFailureReason::CharacterDead;
        return false;
    }

    if (IMMOTargetableInterface::Execute_IsInCombat(OwnerCharacter))
    {
        OutReason = EPlacementFailureReason::CombatActive;
        return false;
    }

    OutReason = EPlacementFailureReason::Success;
    return true;
}

// ============================================================================
// PLACEMENT VALIDATION
// ============================================================================

bool UMMOHousingBridgeComponent::ValidatePlacement(
    FName PieceID,
    const FTransform& PlacementTransform,
    EPlacementFailureReason& OutReason)
{
    if (!OwnerCharacter)
    {
        OutReason = EPlacementFailureReason::CharacterDead;
        return false;
    }

    if (!bIsInBuildMode)
    {
        OutReason = EPlacementFailureReason::BuildModeNotActive;
        return false;
    }

    if (!AvailablePieces.Contains(PieceID))
    {
        OutReason = EPlacementFailureReason::InvalidPiece;
        return false;
    }

    const FBuildingPieceDefinition& Piece = AvailablePieces[PieceID];
    FVector PlacementLocation = PlacementTransform.GetLocation();

    if (!ValidatePlotBoundary(PlacementLocation, OutReason)) return false;
    if (!ValidateGround(PlacementLocation, OutReason)) return false;
    if (!ValidateStructureOverlap(PlacementLocation, Piece.PieceSize, OutReason)) return false;
    if (!ValidateStructureDistance(PlacementLocation, OutReason)) return false;

    OutReason = EPlacementFailureReason::Success;
    return true;
}

bool UMMOHousingBridgeComponent::ValidatePlotBoundary(
    const FVector& PlacementLocation,
    EPlacementFailureReason& OutReason)
{
    FVector OwnerLocation = OwnerCharacter->GetActorLocation();
    float Distance = FVector::Dist(OwnerLocation, PlacementLocation);

    if (Distance > PlotBoundaryRadius)
    {
        OutReason = EPlacementFailureReason::OutOfBounds;
        return false;
    }

    return true;
}

bool UMMOHousingBridgeComponent::ValidateGround(
    const FVector& PlacementLocation,
    EPlacementFailureReason& OutReason)
{
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

    if (!bHit || !Hit.bBlockingHit)
    {
        OutReason = EPlacementFailureReason::NoGround;
        return false;
    }

    return true;
}

// AUDIT FIX: uses the dedicated ECC_BuildingStructure channel instead of
// ECC_Pawn. Checking ECC_Pawn caused two separate failure modes: (1) a player
// or NPC standing near the build site registered as a "structure" and
// incorrectly blocked valid placement, and (2) any structure whose collision
// only blocks ECC_WorldStatic (the default for static architecture meshes,
// including most EBS v10 pieces out of the box) was invisible to this check
// entirely, allowing structures to overlap. See MASTER_CODE_AUDIT.md Finding #7 -
// EBS pieces must be configured to block ECC_BuildingStructure specifically.
bool UMMOHousingBridgeComponent::ValidateStructureOverlap(
    const FVector& PlacementLocation,
    const FVector& PieceSize,
    EPlacementFailureReason& OutReason)
{
    if (!GetWorld())
    {
        OutReason = EPlacementFailureReason::InvalidPiece;
        return false;
    }

    FVector BoxExtent = PieceSize / 2.0f;
    FCollisionShape CollisionBox = FCollisionShape::MakeBox(BoxExtent);

    FCollisionQueryParams OverlapParams;
    OverlapParams.AddIgnoredActor(OwnerCharacter);

    TArray<FOverlapResult> Overlaps;
    GetWorld()->OverlapMultiByChannel(
        Overlaps,
        PlacementLocation,
        FQuat::Identity,
        ECC_BuildingStructure,
        CollisionBox,
        OverlapParams
    );

    if (Overlaps.Num() > 0)
    {
        OutReason = EPlacementFailureReason::OverlappingStructure;
        return false;
    }

    return true;
}

bool UMMOHousingBridgeComponent::ValidateStructureDistance(
    const FVector& PlacementLocation,
    EPlacementFailureReason& OutReason)
{
    if (!GetWorld())
    {
        OutReason = EPlacementFailureReason::InvalidPiece;
        return false;
    }

    FCollisionShape LargeBox = FCollisionShape::MakeBox(FVector(500, 500, 300));

    FCollisionQueryParams NearbyParams;
    NearbyParams.AddIgnoredActor(OwnerCharacter);

    TArray<FOverlapResult> NearbyStructures;
    GetWorld()->OverlapMultiByChannel(
        NearbyStructures,
        PlacementLocation,
        FQuat::Identity,
        ECC_BuildingStructure,
        LargeBox,
        NearbyParams
    );

    for (const FOverlapResult& Overlap : NearbyStructures)
    {
        AActor* OverlapActor = Overlap.GetActor();
        if (!OverlapActor || OverlapActor == OwnerCharacter) continue;

        float DistToStructure = FVector::Dist(PlacementLocation, OverlapActor->GetActorLocation());
        if (DistToStructure < MinDistanceBetweenStructures)
        {
            OutReason = EPlacementFailureReason::TooCloseToStructure;
            return false;
        }
    }

    return true;
}

// ============================================================================
// PLACEMENT REQUEST
// ============================================================================

bool UMMOHousingBridgeComponent::Server_RequestStructurePlacement_Validate(
    FName PieceID,
    FTransform PlacementTransform)
{
    // AUDIT FIX: previously this rejected on "!bIsInBuildMode", which is a
    // gameplay-state race condition, not a cheat signal - a client that
    // exits build mode at the exact moment an in-flight placement RPC
    // arrives would get its connection closed by the engine (a failed
    // _Validate terminates the connection). The only thing worth rejecting
    // here is a malformed/empty PieceID; every real gameplay rule (including
    // build-mode-active) is re-checked in _Implementation via
    // ValidatePlacement() and fails softly with an OnPlacementFailed
    // broadcast instead.
    return !PieceID.IsNone();
}

void UMMOHousingBridgeComponent::Server_RequestStructurePlacement_Implementation(
    FName PieceID,
    FTransform PlacementTransform)
{
    EPlacementFailureReason FailureReason = EPlacementFailureReason::Success;

    if (!ValidatePlacement(PieceID, PlacementTransform, FailureReason))
    {
        OnPlacementFailed.Broadcast(FailureReason);
        return;
    }

    if (!DeductBuildingCosts(PieceID))
    {
        OnPlacementFailed.Broadcast(EPlacementFailureReason::InsufficientResources);
        return;
    }

    if (HousingSubsystem)
    {
        // AUDIT FIX: pass the already-resolved FBuildingPieceDefinition
        // through rather than having the subsystem look PieceID up a second
        // time in a duplicate registry - AvailablePieces on this component
        // stays the single source of truth for piece data.
        const FBuildingPieceDefinition& PieceDef = AvailablePieces[PieceID];
        HousingSubsystem->SpawnStructure(PieceID, PieceDef, PlacementTransform, OwnerCharacter->PlayerUID);
        OnStructurePlaced.Broadcast(PieceID, PlacementTransform);
    }
}

// ============================================================================
// RESOURCE DEDUCTION
// ============================================================================
// AUDIT FIX: the previous version called OwnerInventory->GetItemCountByTag()/
// RemoveItemByTag(). Those methods do not exist on FMMOInventoryItem-based
// inventories - equipment items are identified by an int32 ItemID and carry
// no stack count, so there is no way to represent "37 Stone" as inventory
// entries without exhausting the 20-slot cap after 20 units. Building
// resources are a separate, stackable currency-like concept (the project
// already has a precedent for this: AetherCurrency on FMMOPlayerProfile is a
// plain int32, not an inventory item).
//
// REQUIRED ADDITION to UMMOInventoryComponent (not yet implemented - flagged
// in MASTER_CODE_AUDIT.md as a required file addition):
//
//   UPROPERTY(Replicated) int32 ResourceWood  = 0;
//   UPROPERTY(Replicated) int32 ResourceStone = 0;
//   UPROPERTY(Replicated) int32 ResourceIron  = 0;
//
//   int32 GetResourceWood()  const { return ResourceWood;  }
//   int32 GetResourceStone() const { return ResourceStone; }
//   int32 GetResourceIron()  const { return ResourceIron;  }
//
//   // Server-only. Atomically checks affordability THEN deducts all three
//   // in one call, so there is no separate check-then-remove window for a
//   // future concurrent spend (e.g. a trade or crafting RPC) to race against.
//   bool ConsumeResources(int32 Wood, int32 Stone, int32 Iron)
//   {
//       if (ResourceWood < Wood || ResourceStone < Stone || ResourceIron < Iron)
//           return false;
//       ResourceWood -= Wood; ResourceStone -= Stone; ResourceIron -= Iron;
//       return true;
//   }
// ============================================================================

bool UMMOHousingBridgeComponent::DeductBuildingCosts(FName PieceID)
{
    if (!AvailablePieces.Contains(PieceID) || !OwnerInventory)
    {
        UE_LOG(LogTemp, Error, TEXT("DeductBuildingCosts: Invalid piece or inventory"));
        return false;
    }

    if (GetOwnerRole() != ROLE_Authority)
    {
        UE_LOG(LogTemp, Error, TEXT("DeductBuildingCosts: Not server authority"));
        return false;
    }

    const FBuildingPieceCost& Cost = AvailablePieces[PieceID].Cost;

    // Single atomic call - see REQUIRED ADDITION note above.
    const bool bSuccess = OwnerInventory->ConsumeResources(Cost.WoodRequired, Cost.StoneRequired, Cost.IronRequired);

    if (!bSuccess)
    {
        UE_LOG(LogTemp, Warning, TEXT("Insufficient resources for piece: %s"), *PieceID.ToString());
    }

    return bSuccess;
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

bool UMMOHousingBridgeComponent::CanPlacePiece(FName PieceID) const
{
    if (!AvailablePieces.Contains(PieceID) || !OwnerInventory || !bIsInBuildMode)
    {
        return false;
    }

    const FBuildingPieceCost& Cost = AvailablePieces[PieceID].Cost;

    return Cost.CanAfford(
        OwnerInventory->GetResourceWood(),
        OwnerInventory->GetResourceStone(),
        OwnerInventory->GetResourceIron()
    );
}
