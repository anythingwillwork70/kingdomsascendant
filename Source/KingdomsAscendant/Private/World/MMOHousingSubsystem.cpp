#include "World/MMOHousingSubsystem.h"
#include "World/AMMOPlacedStructure.h"

AMMOPlacedStructure* UMMOHousingSubsystem::SpawnStructure(
    FName PieceID,
    const FBuildingPieceDefinition& PieceDef,
    const FTransform& PlacementTransform,
    const FString& OwnerPlayerUID)
{
    UWorld* World = GetWorld();
    if (!World) return nullptr;

    if (World->GetNetMode() == NM_Client)
    {
        UE_LOG(LogTemp, Error, TEXT("SpawnStructure called on a client - structures must be spawned by the server"));
        return nullptr;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    // PieceDef.PieceClass lets content authors point a specific piece at a
    // custom actor class (e.g. a real EBS v10 piece Blueprint once
    // integrated); falling back to AMMOPlacedStructure keeps every piece
    // spawnable and testable before that integration exists.
    TSubclassOf<AMMOPlacedStructure> ClassToSpawn = PieceDef.PieceClass && PieceDef.PieceClass->IsChildOf(AMMOPlacedStructure::StaticClass())
        ? TSubclassOf<AMMOPlacedStructure>(*PieceDef.PieceClass)
        : AMMOPlacedStructure::StaticClass();

    AMMOPlacedStructure* NewStructure = World->SpawnActor<AMMOPlacedStructure>(ClassToSpawn, PlacementTransform, SpawnParams);
    if (!NewStructure)
    {
        UE_LOG(LogTemp, Error, TEXT("SpawnStructure: SpawnActor failed for piece '%s'"), *PieceID.ToString());
        return nullptr;
    }

    NewStructure->InitializeStructure(PieceID, OwnerPlayerUID, PieceDef.EBSMeshPath);
    PlacedStructures.Add(NewStructure);

    return NewStructure;
}

TArray<AMMOPlacedStructure*> UMMOHousingSubsystem::GetStructuresForPlayer(const FString& PlayerUID) const
{
    TArray<AMMOPlacedStructure*> Result;

    for (AMMOPlacedStructure* Structure : PlacedStructures)
    {
        if (IsValid(Structure) && Structure->OwnerPlayerUID == PlayerUID)
        {
            Result.Add(Structure);
        }
    }

    return Result;
}
