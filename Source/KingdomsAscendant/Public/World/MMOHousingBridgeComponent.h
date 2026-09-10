#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Inventory/MMOInventoryItem.h"
#include "MMOHousingBridgeComponent.generated.h"

// ============================================================================
// COLLISION CHANNEL REQUIREMENT (read before using this component)
// ============================================================================
// Placed structures must NOT be detected on ECC_Pawn (that channel is also
// what players/NPCs block on, causing false-positive overlaps against living
// actors standing near a build site, and false negatives against structures
// whose meshes only block ECC_WorldStatic).
//
// In Project Settings > Collision > Trace Channels, add/rename a channel to
// "BuildingStructure" (this maps to ECC_GameTraceChannel1 below). Every EBS
// piece's static mesh must have its collision preset configured to BLOCK
// this channel (and nothing else needs to). See MASTER_CODE_AUDIT.md
// Finding #7.
// ============================================================================
#define ECC_BuildingStructure ECC_GameTraceChannel1

// ============================================================================
// PLACEMENT FAILURE REASONS (For error feedback)
// ============================================================================

UENUM(BlueprintType)
enum class EPlacementFailureReason : uint8
{
    Success = 0,
    OutOfBounds,
    NoGround,
    OverlappingStructure,
    TooCloseToStructure,
    InsufficientResources,
    InvalidPiece,
    CombatActive,
    CharacterDead,
    BuildModeNotActive
};

// ============================================================================
// BUILDING COST STRUCTURE
// ============================================================================

USTRUCT(BlueprintType)
struct FBuildingPieceCost
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
    int32 WoodRequired = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
    int32 StoneRequired = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
    int32 IronRequired = 0;

    bool CanAfford(int32 Wood, int32 Stone, int32 Iron) const
    {
        return Wood >= WoodRequired && Stone >= StoneRequired && Iron >= IronRequired;
    }
};

// ============================================================================
// BUILDING PIECE DEFINITION
// ============================================================================

USTRUCT(BlueprintType)
struct FBuildingPieceDefinition
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
    FName PieceID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
    TSubclassOf<AActor> PieceClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
    FBuildingPieceCost Cost;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
    FString EBSMeshPath;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
    FVector PieceSize = FVector(100, 100, 200);
};

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBuildModeToggled, bool, bEnabled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStructurePlaced, FName, PieceID, FTransform, Location);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlacementFailed, EPlacementFailureReason, Reason);

// ============================================================================
// MAIN COMPONENT CLASS
// ============================================================================

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MYMMO_API UMMOHousingBridgeComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMMOHousingBridgeComponent();

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ========================================================================
    // BUILD MODE MANAGEMENT
    // ========================================================================

    /**
     * Client-callable (bind to input). Performs a client-side sanity check
     * then requests the change via Server RPC.
     *
     * AUDIT FIX (CRITICAL): the previous version called straight into
     * OnBuildModeActivated()/ExitBuildMode(), both of which early-out unless
     * GetOwnerRole() == ROLE_Authority. Since ToggleBuildMode() itself only
     * ever ran on the client that owns the pawn (IsLocallyControlled() gate),
     * on a dedicated server this meant build mode NEVER actually activated -
     * the authority-gated functions always bailed out immediately. Only a
     * listen server testing on their own character would ever have seen this
     * work, which is exactly the kind of bug that survives solo testing and
     * breaks in the target dedicated-server architecture. Same bug class as
     * AMMOCharacter::ToggleTargetLockMode - see MASTER_CODE_AUDIT.md Finding #1.
     */
    UFUNCTION(BlueprintCallable, Category = "Housing")
    void ToggleBuildMode();

    UFUNCTION(Server, Reliable, WithValidation, Category = "Housing")
    void Server_SetBuildMode(bool bNewBuildMode);

    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation, Category = "Housing")
    void Server_RequestStructurePlacement(FName PieceID, FTransform PlacementTransform);

    UFUNCTION(BlueprintCallable, Category = "Housing")
    bool CanPlacePiece(FName PieceID) const;

    UFUNCTION(BlueprintCallable, Category = "Housing")
    void ExitBuildMode();

    UFUNCTION(BlueprintCallable, Category = "Housing")
    bool IsInBuildMode() const { return bIsInBuildMode; }

    // ========================================================================
    // EVENTS
    // ========================================================================

    UPROPERTY(BlueprintAssignable, Category = "Housing|Events")
    FOnBuildModeToggled OnBuildModeToggled;

    UPROPERTY(BlueprintAssignable, Category = "Housing|Events")
    FOnStructurePlaced OnStructurePlaced;

    UPROPERTY(BlueprintAssignable, Category = "Housing|Events")
    FOnPlacementFailed OnPlacementFailed;

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Housing")
    bool bIsInBuildMode = false;

    UPROPERTY(EditDefaultsOnly, Category = "Housing|Config")
    TMap<FName, FBuildingPieceDefinition> AvailablePieces;

    UPROPERTY(EditDefaultsOnly, Category = "Housing|Config")
    float PlotBoundaryRadius = 1000.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Housing|Config")
    float MinDistanceBetweenStructures = 50.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Housing|Config")
    float BuildModeLoweredSpeed = 300.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Housing|Config")
    float CameraObstructionCheckDistance = 50.0f;

protected:
    UPROPERTY()
    class UMMOInventoryComponent* OwnerInventory;

    UPROPERTY()
    class AMMOCharacter* OwnerCharacter;

    UPROPERTY()
    class UMMOHousingSubsystem* HousingSubsystem;

    UPROPERTY()
    float StoredCombatSpeed = 600.0f;

    // ========================================================================
    // INTERNAL FUNCTIONS
    // ========================================================================

    // AUDIT FIX: this function was called from BeginPlay() in the .cpp but
    // was never declared here, which would fail to compile. Declared now.
    void InitializeDefaultPieces();

    void OnBuildModeActivated();
    void OnBuildModeDeactivated();

    bool ValidatePlacement(FName PieceID, const FTransform& PlacementTransform, EPlacementFailureReason& OutReason);
    bool DeductBuildingCosts(FName PieceID);

    bool ValidateCharacterState(EPlacementFailureReason& OutReason);
    bool ValidatePlotBoundary(const FVector& PlacementLocation, EPlacementFailureReason& OutReason);
    bool ValidateGround(const FVector& PlacementLocation, EPlacementFailureReason& OutReason);
    bool ValidateStructureOverlap(const FVector& PlacementLocation, const FVector& PieceSize, EPlacementFailureReason& OutReason);
    bool ValidateStructureDistance(const FVector& PlacementLocation, EPlacementFailureReason& OutReason);
};
