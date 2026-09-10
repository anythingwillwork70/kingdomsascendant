#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MMOPlayerProfile.generated.h"

USTRUCT(BlueprintType)
struct FMMOPlayerProfile
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite, Category = "Profile")
    FString PlayerUID;

    UPROPERTY(BlueprintReadWrite, Category = "Profile")
    FString CharacterName;

    UPROPERTY(BlueprintReadWrite, Category = "Profile")
    FGameplayTag ClassTag;

    UPROPERTY(BlueprintReadWrite, Category = "Profile")
    FGameplayTag SpecTag;

    UPROPERTY(BlueprintReadWrite, Category = "Profile")
    int32 CharacterLevel = 1;

    UPROPERTY(BlueprintReadWrite, Category = "Profile")
    int32 ExperiencePoints = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Profile")
    int32 AetherCurrency = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Profile|Progression")
    TArray<FString> CompletedQuestIDs;

    UPROPERTY(BlueprintReadWrite, Category = "Profile|Progression")
    float PlayTimeSessions = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Profile")
    int64 LastPlayedTimestamp = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Profile")
    int32 SaveVersion = 1;
};
