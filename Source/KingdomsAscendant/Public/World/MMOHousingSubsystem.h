#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "World/MMOHousingBridgeComponent.h" // FBuildingPieceDefinition
#include "MMOHousingSubsystem.generated.h"

UCLASS()
class MYMMO_API UMMOHousingSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    /**
     * Spawns the placed-structure actor for a piece. Server-only (structure
     * spawning must be authoritative - this is only ever reached today via
     * UMMOHousingBridgeComponent::Server_RequestStructurePlacement_Implementation,
     * which already runs on the server).
     *
     * PieceDef is passed in by the caller (resolved from
     * UMMOHousingBridgeComponent::AvailablePieces) rather than looked up
     * again here, so there is exactly one place piece definitions live -
     * avoids a second, potentially-desynced copy of the same data.
     */
    UFUNCTION(BlueprintCallable, Category = "Housing")
    class AMMOPlacedStructure* SpawnStructure(FName PieceID, const FBuildingPieceDefinition& PieceDef, const FTransform& PlacementTransform, const FString& OwnerPlayerUID);

    /** All structures placed by a given player, for later query (e.g. plot limits, save-on-logout). */
    UFUNCTION(BlueprintCallable, Category = "Housing")
    TArray<AMMOPlacedStructure*> GetStructuresForPlayer(const FString& PlayerUID) const;

protected:
    UPROPERTY()
    TArray<class AMMOPlacedStructure*> PlacedStructures;
};
