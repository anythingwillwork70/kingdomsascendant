#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "Quest/MMOQuestTypes.h"
#include "MMOQuestComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestAccepted, FName, QuestID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestCompleted, FName, QuestID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestAbandoned, FName, QuestID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQuestProgressUpdated, FName, QuestID, int32, NewProgress);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MYMMO_API UMMOQuestComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMMOQuestComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Quest")
    TArray<FActiveQuestData> ActiveQuests;

    // NOTE (see MASTER_CODE_AUDIT.md Finding #12): this is TArray<FName>
    // here, matching the quest system's natural key type, but
    // FMMOPlayerProfile::CompletedQuestIDs is TArray<FString> for save-file
    // portability/readability. MMOPersistenceSubsystem performs the explicit
    // FName<->FString conversion at the save/load boundary - do not "fix" the
    // mismatch by changing one type to match the other; they serve different
    // purposes (fast tag-like comparison at runtime vs. a stable serialized
    // format).
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Quest")
    TArray<FName> CompletedQuests;

    UPROPERTY(EditDefaultsOnly, Category = "Quest|Config")
    UDataTable* DefaultQuestDataTable = nullptr;

    /** Populates the internal lookup cache from a quest DataTable. Safe to call multiple times (e.g. to layer Level 11-20 quests on top of Level 1-10). */
    UFUNCTION(BlueprintCallable, Category = "Quest")
    void LoadQuestData(UDataTable* QuestDataTable);

    UFUNCTION(BlueprintCallable, Category = "Quest")
    bool HasCompletedQuest(FName QuestID) const { return CompletedQuests.Contains(QuestID); }

    UFUNCTION(BlueprintCallable, Category = "Quest")
    bool HasActiveQuest(FName QuestID) const;

    /** Client-callable entry point (bind to quest-giver UI). Routes to Server_AcceptQuest. */
    UFUNCTION(BlueprintCallable, Category = "Quest")
    void RequestAcceptQuest(FName QuestID);

    UFUNCTION(Server, Reliable, WithValidation, Category = "Quest")
    void Server_AcceptQuest(FName QuestID);

    UFUNCTION(BlueprintCallable, Category = "Quest")
    void RequestAbandonQuest(FName QuestID);

    UFUNCTION(Server, Reliable, WithValidation, Category = "Quest")
    void Server_AbandonQuest(FName QuestID);

    /**
     * Fired by gameplay systems (kill confirmation, item pickup, area trigger,
     * NPC dialogue) whenever something happens that might advance a quest
     * objective. Server-authoritative; safe to call redundantly.
     */
    UFUNCTION(Server, Reliable, WithValidation, Category = "Quest")
    void Server_NotifyGameplayEvent(FGameplayTag EventTag, int32 Quantity);

    UPROPERTY(BlueprintAssignable, Category = "Quest|Events")
    FOnQuestAccepted OnQuestAccepted;

    UPROPERTY(BlueprintAssignable, Category = "Quest|Events")
    FOnQuestCompleted OnQuestCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Quest|Events")
    FOnQuestAbandoned OnQuestAbandoned;

    UPROPERTY(BlueprintAssignable, Category = "Quest|Events")
    FOnQuestProgressUpdated OnQuestProgressUpdated;

protected:
    /**
     * Local (non-replicated) cache of quest definitions keyed by QuestID.
     * Built from DataTable assets, which are static content present
     * identically on server and every client - definitions themselves never
     * need to replicate, only the player's progress through them
     * (ActiveQuests/CompletedQuests, both replicated above) does.
     */
    UPROPERTY()
    TMap<FName, FQuestObjectiveDefinition> CachedQuestData;

    virtual void BeginPlay() override;

    /** AUDIT FIX (original Gemini bug): this is now actually called, from Server_NotifyGameplayEvent, after every progress update - not left defined-but-uncalled. */
    void CheckQuestCompletion(FActiveQuestData& Quest);

    void AwardQuestRewards(const FQuestObjectiveDefinition& QuestDef);
};
