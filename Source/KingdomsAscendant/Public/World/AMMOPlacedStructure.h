#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AMMOPlacedStructure.generated.h"

/**
 * PLACEHOLDER BRIDGE ACTOR - read before replacing.
 *
 * This is a minimal, fully-functional stand-in for whatever Easy Building
 * System v10 actually spawns when a piece is placed. It exists so the
 * housing feature is genuinely testable end-to-end (place a wall, see a
 * wall, it blocks ECC_BuildingStructure so the overlap/distance checks in
 * MMOHousingBridgeComponent have something real to detect) WITHOUT the EBS
 * v10 plugin needing to be imported first.
 *
 * Per IMPLEMENTATION_GUIDE_EBS_HARDLOCK.md, once EBS v10 is actually
 * integrated, real placement should go through EBS's own piece actor
 * classes (which bring their own snapping/connection-socket logic this
 * class does not attempt to replicate) - at that point
 * UMMOHousingSubsystem::SpawnStructure's PieceClass-not-set fallback path
 * below should be pointed at the EBS piece Blueprint/class instead of this
 * one. This class is not meant to be the final shipped structure actor.
 */
UCLASS()
class MYMMO_API AMMOPlacedStructure : public AActor
{
    GENERATED_BODY()

public:
    AMMOPlacedStructure();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(VisibleAnywhere, Category = "Structure")
    class UStaticMeshComponent* StructureMesh;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Structure")
    FName PieceID;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Structure")
    FString OwnerPlayerUID;

    /** Server-only. Sets identity and attempts to load the mesh at MeshSoftPath (a /Game/... content path, as stored in FBuildingPieceDefinition::EBSMeshPath). */
    UFUNCTION(BlueprintCallable, Category = "Structure")
    void InitializeStructure(FName InPieceID, const FString& InOwnerPlayerUID, const FString& MeshSoftPath);
};
