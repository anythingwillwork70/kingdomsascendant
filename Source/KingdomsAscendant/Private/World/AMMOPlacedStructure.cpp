#include "World/AMMOPlacedStructure.h"
#include "World/MMOHousingBridgeComponent.h" // for ECC_BuildingStructure
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/SoftObjectPath.h"
#include "Engine/StaticMesh.h"

AMMOPlacedStructure::AMMOPlacedStructure()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    NetDormancy = DORM_Initial; // structures are static once placed - no need to keep them net-relevant every frame

    StructureMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StructureMesh"));
    RootComponent = StructureMesh;

    // Block the dedicated building-structure trace channel (see
    // MASTER_CODE_AUDIT.md Finding #7) so MMOHousingBridgeComponent's
    // overlap/distance checks can actually detect this structure. Also
    // block WorldStatic so players/NPCs collide with it physically.
    StructureMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    StructureMesh->SetCollisionResponseToAllChannels(ECR_Block);
    StructureMesh->SetCollisionResponseToChannel(ECC_BuildingStructure, ECR_Block);
    StructureMesh->SetMobility(EComponentMobility::Static);
}

void AMMOPlacedStructure::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AMMOPlacedStructure, PieceID);
    DOREPLIFETIME(AMMOPlacedStructure, OwnerPlayerUID);
}

void AMMOPlacedStructure::InitializeStructure(FName InPieceID, const FString& InOwnerPlayerUID, const FString& MeshSoftPath)
{
    if (GetLocalRole() != ROLE_Authority)
    {
        UE_LOG(LogTemp, Warning, TEXT("InitializeStructure called on non-authority - ignored"));
        return;
    }

    PieceID = InPieceID;
    OwnerPlayerUID = InOwnerPlayerUID;

    if (!MeshSoftPath.IsEmpty())
    {
        // Synchronous load is acceptable here: structure placement is a
        // deliberate, infrequent player action (not a hot path), and the
        // mesh must be visible on the server for collision to make sense
        // before the actor replicates to clients.
        UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshSoftPath);
        if (Mesh && StructureMesh)
        {
            StructureMesh->SetStaticMesh(Mesh);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("InitializeStructure: failed to load mesh at '%s' for piece '%s' - spawning with no visible mesh (collision still applies)"),
                *MeshSoftPath, *InPieceID.ToString());
        }
    }
}
