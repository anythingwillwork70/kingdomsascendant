#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "MMOQuestTypes.generated.h"

UENUM(BlueprintType)
enum class EQuestObjectiveType : uint8
{
    KillTarget UMETA(DisplayName = "Kill Target"),
    GatherItem UMETA(DisplayName = "Gather Item"),
    ExploreArea UMETA(DisplayName = "Explore Area"),
    TalkToNPC UMETA(DisplayName = "Talk to NPC")
};

UENUM(BlueprintType)
enum class EQuestStatus : uint8
{
    Available,
    Active,
    Completed,
    Failed
};

USTRUCT(BlueprintType)
struct FQuestObjectiveDefinition : public FTableRowBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
    FName QuestID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
    FText QuestTitle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
    FText QuestDescription;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|Objectives")
    EQuestObjectiveType ObjectiveType;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|Objectives")
    FGameplayTag ObjectiveTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|Objectives")
    int32 RequiredCount = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|Requirements")
    int32 MinimumLevel = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|Requirements")
    int32 MaximumLevel = 20;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|Rewards")
    int32 RewardExperience = 100;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|Rewards")
    int32 RewardAether = 50;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|Rewards")
    int32 RewardGearTier = 1;
};

USTRUCT(BlueprintType)
struct FActiveQuestData
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite, Category = "Quest")
    FName QuestID;

    UPROPERTY(BlueprintReadWrite, Category = "Quest")
    FText QuestTitle;

    UPROPERTY(BlueprintReadWrite, Category = "Quest")
    EQuestStatus Status = EQuestStatus::Active;

    UPROPERTY(BlueprintReadWrite, Category = "Quest")
    int32 CurrentProgress = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Quest")
    int32 TargetProgress = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Quest")
    float StartTime = 0.0f;
};
