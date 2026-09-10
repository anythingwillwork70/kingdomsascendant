#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Dom/JsonObject.h"
#include "Persistence/MMOPlayerProfile.h"
#include "MMOPersistenceSubsystem.generated.h"

UCLASS()
class MYMMO_API UMMOPersistenceSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    static constexpr int32 CURRENT_SAVE_VERSION = 1;

    UFUNCTION(BlueprintCallable, Category = "Persistence")
    bool SavePlayerProfile(const FMMOPlayerProfile& Profile);

    UFUNCTION(BlueprintCallable, Category = "Persistence")
    bool LoadPlayerProfile(const FString& PlayerUID, FMMOPlayerProfile& OutProfile);

    UFUNCTION(BlueprintCallable, Category = "Persistence")
    bool ProfileExists(const FString& PlayerUID) const;

protected:
    FString GetProfileFilePath(const FString& PlayerUID) const;

    TSharedPtr<FJsonObject> SerializeProfileToJSON(const FMMOPlayerProfile& Profile) const;
    bool DeserializeProfileFromJSON(const TSharedPtr<FJsonObject>& JsonObject, FMMOPlayerProfile& OutProfile) const;

    /** Applies field-by-field upgrades for profiles saved by an older SaveVersion. Called automatically by LoadPlayerProfile before returning. */
    void MigrateProfileIfNeeded(FMMOPlayerProfile& Profile) const;
};
