#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Inventory/MMOInventoryItem.h"
#include "MMOLootItem.generated.h"

UCLASS()
class MYMMO_API AMMOLootItem : public AActor
{
    GENERATED_BODY()

public:
    AMMOLootItem();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(VisibleAnywhere, Category = "Loot")
    class USphereComponent* PickupSphere;

    UPROPERTY(VisibleAnywhere, Category = "Loot")
    class UStaticMeshComponent* ItemMesh;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Loot")
    FMMOInventoryItem ContainedItem;

    UPROPERTY(EditDefaultsOnly, Category = "Loot|Config")
    float AutoPickupRadius = 200.0f;

    /** Seconds before this loot despawns unclaimed. 300 = 5 minutes. */
    UPROPERTY(EditDefaultsOnly, Category = "Loot|Config")
    float AutoDespawnSeconds = 300.0f;

    /** Server-only. Sets the item this pickup grants and (re)positions it. */
    UFUNCTION(BlueprintCallable, Category = "Loot")
    void InitializeLootItem(const FMMOInventoryItem& Item, FVector Location);

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void OnPickupSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    void TryPickupItem(class AMMOCharacter* PickingCharacter);
    void SetupLootVisuals();
    void DespawnUnclaimed() { Destroy(); }

    FTimerHandle DespawnTimerHandle;
};
