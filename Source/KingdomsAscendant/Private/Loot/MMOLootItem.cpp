#include "Loot/MMOLootItem.h"
#include "Characters/MMOCharacter.h"
#include "Inventory/MMOInventoryComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Materials/MaterialInstanceDynamic.h"

AMMOLootItem::AMMOLootItem()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
    PickupSphere->SetSphereRadius(AutoPickupRadius);
    PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    RootComponent = PickupSphere;

    ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
    ItemMesh->SetupAttachment(RootComponent);
    ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AMMOLootItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AMMOLootItem, ContainedItem);
}

void AMMOLootItem::BeginPlay()
{
    Super::BeginPlay();

    PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &AMMOLootItem::OnPickupSphereBeginOverlap);

    SetupLootVisuals();

    if (GetLocalRole() == ROLE_Authority)
    {
        // AActor::Destroy() (not ConditionalBeginDestroy, which is a UObject
        // GC-internal method, not a gameplay despawn call) - Destroy() also
        // correctly replicates the actor's removal to clients.
        GetWorldTimerManager().SetTimer(DespawnTimerHandle, this, &AMMOLootItem::DespawnUnclaimed, AutoDespawnSeconds, false);
    }
}

void AMMOLootItem::InitializeLootItem(const FMMOInventoryItem& Item, FVector Location)
{
    if (GetLocalRole() != ROLE_Authority)
    {
        UE_LOG(LogTemp, Warning, TEXT("InitializeLootItem called on non-authority - ignored"));
        return;
    }

    ContainedItem = Item;
    SetActorLocation(Location);
    SetupLootVisuals();
}

void AMMOLootItem::OnPickupSphereBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    // AUDIT-PATTERN FIX applied proactively: overlap events on a replicated
    // actor with QueryOnly collision fire on whichever machine processes the
    // physics/overlap query, which in practice can include clients. Pickup
    // (destroying the actor, granting inventory) MUST only ever be decided
    // by the server - clients merely predict/see the result via replication
    // and eventual Destroy(). Without this guard a client could otherwise
    // locally "pick up" loot with no server-side effect, or - worse - a
    // modified client could attempt to trigger pickup logic that mutates
    // state it has no authority over.
    if (GetLocalRole() != ROLE_Authority) return;

    if (AMMOCharacter* Character = Cast<AMMOCharacter>(OtherActor))
    {
        TryPickupItem(Character);
    }
}

void AMMOLootItem::TryPickupItem(AMMOCharacter* PickingCharacter)
{
    if (!PickingCharacter || !PickingCharacter->InventoryComponent) return;

    if (PickingCharacter->InventoryComponent->AddItem(ContainedItem))
    {
        GetWorldTimerManager().ClearTimer(DespawnTimerHandle);
        Destroy();
    }
    // If AddItem fails (inventory full), the loot simply remains in the
    // world for the player to come back for - no error state needed here.
}

void AMMOLootItem::SetupLootVisuals()
{
    if (!ItemMesh) return;

    UMaterialInstanceDynamic* DynMat = ItemMesh->CreateAndSetMaterialInstanceDynamic(0);
    if (!DynMat) return;

    FLinearColor RarityColor = FLinearColor::White;
    switch (ContainedItem.ItemRarity)
    {
        case EItemRarity::Common:    RarityColor = FLinearColor(0.6f, 0.6f, 0.6f); break; // grey
        case EItemRarity::Uncommon:  RarityColor = FLinearColor(0.1f, 0.8f, 0.1f); break; // green
        case EItemRarity::Rare:      RarityColor = FLinearColor(0.1f, 0.4f, 1.0f); break; // blue
        case EItemRarity::Epic:      RarityColor = FLinearColor(0.6f, 0.1f, 0.9f); break; // purple
        case EItemRarity::Legendary: RarityColor = FLinearColor(1.0f, 0.55f, 0.0f); break; // orange
    }

    DynMat->SetVectorParameterValue(TEXT("RarityColor"), RarityColor);
}
