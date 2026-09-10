#include "Quest/MMOQuestComponent.h"
#include "Characters/MMOCharacter.h"
#include "Player/MMOPlayerState.h"
#include "Net/UnrealNetwork.h"

UMMOQuestComponent::UMMOQuestComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicated(true);
}

void UMMOQuestComponent::BeginPlay()
{
    Super::BeginPlay();

    if (DefaultQuestDataTable)
    {
        LoadQuestData(DefaultQuestDataTable);
    }
}

void UMMOQuestComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UMMOQuestComponent, ActiveQuests);
    DOREPLIFETIME(UMMOQuestComponent, CompletedQuests);
}

void UMMOQuestComponent::LoadQuestData(UDataTable* QuestDataTable)
{
    if (!QuestDataTable) return;

    TArray<FQuestObjectiveDefinition*> Rows;
    QuestDataTable->GetAllRows<FQuestObjectiveDefinition>(TEXT("LoadQuestData"), Rows);

    for (FQuestObjectiveDefinition* Row : Rows)
    {
        if (Row && !Row->QuestID.IsNone())
        {
            CachedQuestData.Add(Row->QuestID, *Row);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("LoadQuestData: cached %d quest definitions (table now holds %d total)"),
        Rows.Num(), CachedQuestData.Num());
}

bool UMMOQuestComponent::HasActiveQuest(FName QuestID) const
{
    return ActiveQuests.ContainsByPredicate([QuestID](const FActiveQuestData& Q) { return Q.QuestID == QuestID; });
}

// ============================================================================
// ACCEPT QUEST
// ============================================================================

void UMMOQuestComponent::RequestAcceptQuest(FName QuestID)
{
    Server_AcceptQuest(QuestID);
}

bool UMMOQuestComponent::Server_AcceptQuest_Validate(FName QuestID)
{
    return !QuestID.IsNone();
}

void UMMOQuestComponent::Server_AcceptQuest_Implementation(FName QuestID)
{
    if (HasActiveQuest(QuestID))
    {
        UE_LOG(LogTemp, Warning, TEXT("Server_AcceptQuest: '%s' already active"), *QuestID.ToString());
        return;
    }

    if (HasCompletedQuest(QuestID))
    {
        UE_LOG(LogTemp, Warning, TEXT("Server_AcceptQuest: '%s' already completed"), *QuestID.ToString());
        return;
    }

    const FQuestObjectiveDefinition* Def = CachedQuestData.Find(QuestID);
    if (!Def)
    {
        UE_LOG(LogTemp, Warning, TEXT("Server_AcceptQuest: unknown quest '%s' (not in any loaded DataTable)"), *QuestID.ToString());
        return;
    }

    AMMOCharacter* OwnerChar = Cast<AMMOCharacter>(GetOwner());
    const int32 CharLevel = OwnerChar ? OwnerChar->CharacterLevel : 1;

    if (CharLevel < Def->MinimumLevel || CharLevel > Def->MaximumLevel)
    {
        UE_LOG(LogTemp, Warning, TEXT("Server_AcceptQuest: '%s' requires level %d-%d, character is %d"),
            *QuestID.ToString(), Def->MinimumLevel, Def->MaximumLevel, CharLevel);
        return;
    }

    FActiveQuestData NewQuest;
    NewQuest.QuestID = Def->QuestID;
    NewQuest.QuestTitle = Def->QuestTitle;
    NewQuest.Status = EQuestStatus::Active;
    NewQuest.CurrentProgress = 0;
    NewQuest.TargetProgress = Def->RequiredCount;
    NewQuest.StartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    ActiveQuests.Add(NewQuest);
    OnQuestAccepted.Broadcast(QuestID);
}

// ============================================================================
// ABANDON QUEST
// ============================================================================

void UMMOQuestComponent::RequestAbandonQuest(FName QuestID)
{
    Server_AbandonQuest(QuestID);
}

bool UMMOQuestComponent::Server_AbandonQuest_Validate(FName QuestID)
{
    return !QuestID.IsNone();
}

void UMMOQuestComponent::Server_AbandonQuest_Implementation(FName QuestID)
{
    const int32 RemovedCount = ActiveQuests.RemoveAll([QuestID](const FActiveQuestData& Q) { return Q.QuestID == QuestID; });

    if (RemovedCount > 0)
    {
        OnQuestAbandoned.Broadcast(QuestID);
    }
}

// ============================================================================
// PROGRESS / COMPLETION
// ============================================================================

bool UMMOQuestComponent::Server_NotifyGameplayEvent_Validate(FGameplayTag EventTag, int32 Quantity)
{
    return EventTag.IsValid() && Quantity > 0;
}

void UMMOQuestComponent::Server_NotifyGameplayEvent_Implementation(FGameplayTag EventTag, int32 Quantity)
{
    for (FActiveQuestData& Quest : ActiveQuests)
    {
        if (Quest.Status != EQuestStatus::Active) continue;

        const FQuestObjectiveDefinition* Def = CachedQuestData.Find(Quest.QuestID);
        if (!Def) continue;

        if (Def->ObjectiveTag != EventTag) continue;

        Quest.CurrentProgress = FMath::Min(Quest.CurrentProgress + Quantity, Quest.TargetProgress);
        OnQuestProgressUpdated.Broadcast(Quest.QuestID, Quest.CurrentProgress);

        // AUDIT FIX (original Gemini bug, see conversation history): this
        // completion check previously existed as a function that nothing
        // ever called, so quests could reach 100% progress and never
        // actually complete. It is now invoked unconditionally after every
        // progress update, for every quest whose objective just matched.
        CheckQuestCompletion(Quest);
    }
}

void UMMOQuestComponent::CheckQuestCompletion(FActiveQuestData& Quest)
{
    if (Quest.Status != EQuestStatus::Active) return;
    if (Quest.CurrentProgress < Quest.TargetProgress) return;

    const FQuestObjectiveDefinition* Def = CachedQuestData.Find(Quest.QuestID);
    if (!Def)
    {
        UE_LOG(LogTemp, Error, TEXT("CheckQuestCompletion: '%s' hit target progress but has no cached definition - cannot award rewards"), *Quest.QuestID.ToString());
        return;
    }

    Quest.Status = EQuestStatus::Completed;

    AwardQuestRewards(*Def);

    CompletedQuests.AddUnique(Quest.QuestID);
    ActiveQuests.RemoveAll([QuestID = Quest.QuestID](const FActiveQuestData& Q) { return Q.QuestID == QuestID; });

    OnQuestCompleted.Broadcast(Def->QuestID);
}

void UMMOQuestComponent::AwardQuestRewards(const FQuestObjectiveDefinition& QuestDef)
{
    AMMOCharacter* OwnerChar = Cast<AMMOCharacter>(GetOwner());
    if (!OwnerChar) return;

    AMMOPlayerState* PS = OwnerChar->GetPlayerState<AMMOPlayerState>();
    if (PS)
    {
        PS->Server_AddExperience(QuestDef.RewardExperience);
        PS->Server_AddAether(QuestDef.RewardAether);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("AwardQuestRewards: no AMMOPlayerState found - XP/Aether not granted for '%s'"), *QuestDef.QuestID.ToString());
    }

    // RewardGearTier: the vertical slice awards XP/Aether directly here but
    // deliberately does NOT auto-roll a piece of loot from RewardGearTier -
    // that's the loot table's job (see MMOLootItem / DT_Loot_*), triggered
    // by whatever gameplay moment the quest's final objective represents
    // (e.g. a chest opening, an NPC handing over a reward). Wiring a specific
    // quest to a specific loot roll is a content/level-design decision per
    // quest, not something this generic component should hardcode.
    UE_LOG(LogTemp, Log, TEXT("Quest '%s' completed: +%d XP, +%d Aether (Tier %d gear reward not auto-rolled - see loot table)"),
        *QuestDef.QuestID.ToString(), QuestDef.RewardExperience, QuestDef.RewardAether, QuestDef.RewardGearTier);
}
