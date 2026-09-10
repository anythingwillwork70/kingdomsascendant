#include "Persistence/MMOPersistenceSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "HAL/PlatformFilemanager.h"

FString UMMOPersistenceSubsystem::GetProfileFilePath(const FString& PlayerUID) const
{
    // Sanitize the UID before using it as a filename - PlayerUID is
    // ultimately attacker-influenced input (it travels through
    // Server_SetPlayerIdentity from client-originated data), and an
    // unsanitized UID containing path separators or ".." could otherwise be
    // used to write/read files outside the Profiles directory.
    FString SafeUID = PlayerUID;
    SafeUID.ReplaceInline(TEXT(".."), TEXT("_"));
    SafeUID.ReplaceInline(TEXT("/"), TEXT("_"));
    SafeUID.ReplaceInline(TEXT("\\"), TEXT("_"));
    SafeUID.ReplaceInline(TEXT(":"), TEXT("_"));

    return FPaths::ProjectSavedDir() / TEXT("Profiles") / (SafeUID + TEXT(".json"));
}

bool UMMOPersistenceSubsystem::ProfileExists(const FString& PlayerUID) const
{
    return FPlatformFileManager::Get().GetPlatformFile().FileExists(*GetProfileFilePath(PlayerUID));
}

bool UMMOPersistenceSubsystem::SavePlayerProfile(const FMMOPlayerProfile& Profile)
{
    if (Profile.PlayerUID.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("SavePlayerProfile: refusing to save with empty PlayerUID"));
        return false;
    }

    TSharedPtr<FJsonObject> JsonObject = SerializeProfileToJSON(Profile);

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    if (!FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
    {
        UE_LOG(LogTemp, Error, TEXT("SavePlayerProfile: JSON serialization failed for UID '%s'"), *Profile.PlayerUID);
        return false;
    }

    const FString FilePath = GetProfileFilePath(Profile.PlayerUID);

    if (!FFileHelper::SaveStringToFile(OutputString, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("SavePlayerProfile: failed to write '%s'"), *FilePath);
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("SavePlayerProfile: saved '%s' (%d bytes)"), *FilePath, OutputString.Len());
    return true;
}

bool UMMOPersistenceSubsystem::LoadPlayerProfile(const FString& PlayerUID, FMMOPlayerProfile& OutProfile)
{
    const FString FilePath = GetProfileFilePath(PlayerUID);

    FString FileContents;
    if (!FFileHelper::LoadFileToString(FileContents, *FilePath))
    {
        UE_LOG(LogTemp, Warning, TEXT("LoadPlayerProfile: no profile found at '%s'"), *FilePath);
        return false;
    }

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContents);
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("LoadPlayerProfile: malformed JSON in '%s'"), *FilePath);
        return false;
    }

    if (!DeserializeProfileFromJSON(JsonObject, OutProfile))
    {
        UE_LOG(LogTemp, Error, TEXT("LoadPlayerProfile: failed to deserialize '%s'"), *FilePath);
        return false;
    }

    MigrateProfileIfNeeded(OutProfile);

    return true;
}

TSharedPtr<FJsonObject> UMMOPersistenceSubsystem::SerializeProfileToJSON(const FMMOPlayerProfile& Profile) const
{
    TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();

    JsonObject->SetStringField(TEXT("PlayerUID"), Profile.PlayerUID);
    JsonObject->SetStringField(TEXT("CharacterName"), Profile.CharacterName);
    JsonObject->SetStringField(TEXT("ClassTag"), Profile.ClassTag.ToString());
    JsonObject->SetStringField(TEXT("SpecTag"), Profile.SpecTag.ToString());
    JsonObject->SetNumberField(TEXT("CharacterLevel"), Profile.CharacterLevel);
    JsonObject->SetNumberField(TEXT("ExperiencePoints"), Profile.ExperiencePoints);
    JsonObject->SetNumberField(TEXT("AetherCurrency"), Profile.AetherCurrency);
    JsonObject->SetNumberField(TEXT("PlayTimeSessions"), Profile.PlayTimeSessions);
    JsonObject->SetNumberField(TEXT("LastPlayedTimestamp"), static_cast<double>(Profile.LastPlayedTimestamp));
    JsonObject->SetNumberField(TEXT("SaveVersion"), CURRENT_SAVE_VERSION);

    TArray<TSharedPtr<FJsonValue>> QuestArray;
    for (const FString& QuestID : Profile.CompletedQuestIDs)
    {
        QuestArray.Add(MakeShared<FJsonValueString>(QuestID));
    }
    JsonObject->SetArrayField(TEXT("CompletedQuestIDs"), QuestArray);

    return JsonObject;
}

bool UMMOPersistenceSubsystem::DeserializeProfileFromJSON(const TSharedPtr<FJsonObject>& JsonObject, FMMOPlayerProfile& OutProfile) const
{
    if (!JsonObject.IsValid()) return false;

    JsonObject->TryGetStringField(TEXT("PlayerUID"), OutProfile.PlayerUID);
    JsonObject->TryGetStringField(TEXT("CharacterName"), OutProfile.CharacterName);

    FString ClassTagStr, SpecTagStr;
    JsonObject->TryGetStringField(TEXT("ClassTag"), ClassTagStr);
    JsonObject->TryGetStringField(TEXT("SpecTag"), SpecTagStr);
    OutProfile.ClassTag = FGameplayTag::RequestGameplayTag(FName(*ClassTagStr), false);
    OutProfile.SpecTag = FGameplayTag::RequestGameplayTag(FName(*SpecTagStr), false);

    int32 CharacterLevel = 1, ExperiencePoints = 0, AetherCurrency = 0, SaveVersion = 1;
    JsonObject->TryGetNumberField(TEXT("CharacterLevel"), CharacterLevel);
    JsonObject->TryGetNumberField(TEXT("ExperiencePoints"), ExperiencePoints);
    JsonObject->TryGetNumberField(TEXT("AetherCurrency"), AetherCurrency);
    JsonObject->TryGetNumberField(TEXT("SaveVersion"), SaveVersion);
    OutProfile.CharacterLevel = CharacterLevel;
    OutProfile.ExperiencePoints = ExperiencePoints;
    OutProfile.AetherCurrency = AetherCurrency;
    OutProfile.SaveVersion = SaveVersion;

    double PlayTimeSessions = 0.0;
    JsonObject->TryGetNumberField(TEXT("PlayTimeSessions"), PlayTimeSessions);
    OutProfile.PlayTimeSessions = static_cast<float>(PlayTimeSessions);

    double LastPlayedTimestamp = 0.0;
    JsonObject->TryGetNumberField(TEXT("LastPlayedTimestamp"), LastPlayedTimestamp);
    OutProfile.LastPlayedTimestamp = static_cast<int64>(LastPlayedTimestamp);

    // AUDIT FIX (Finding #12): CompletedQuestIDs is TArray<FString> on the
    // saved profile but TArray<FName> on the live UMMOQuestComponent. JSON
    // string values convert directly to FString here (this is the boundary
    // where that's appropriate); AMMOCharacterSpawner::SetupLoadedCharacter
    // performs the FString -> FName conversion when populating the live
    // quest component from this profile.
    const TArray<TSharedPtr<FJsonValue>>* QuestArray = nullptr;
    if (JsonObject->TryGetArrayField(TEXT("CompletedQuestIDs"), QuestArray) && QuestArray)
    {
        OutProfile.CompletedQuestIDs.Empty(QuestArray->Num());
        for (const TSharedPtr<FJsonValue>& Value : *QuestArray)
        {
            FString QuestIDStr;
            if (Value.IsValid() && Value->TryGetString(QuestIDStr))
            {
                OutProfile.CompletedQuestIDs.Add(QuestIDStr);
            }
        }
    }

    return true;
}

void UMMOPersistenceSubsystem::MigrateProfileIfNeeded(FMMOPlayerProfile& Profile) const
{
    // No migrations needed yet - SaveVersion has never changed. This is the
    // single place future save-format changes get a conversion step, keyed
    // off Profile.SaveVersion, e.g.:
    //
    //   if (Profile.SaveVersion < 2) { /* upgrade v1 -> v2 fields */ }
    //
    // Left as an explicit no-op rather than omitted, so the next engineer
    // who bumps CURRENT_SAVE_VERSION has an obvious place to add the step
    // instead of needing to invent this function from scratch.

    if (Profile.SaveVersion < CURRENT_SAVE_VERSION)
    {
        UE_LOG(LogTemp, Log, TEXT("MigrateProfileIfNeeded: profile '%s' is SaveVersion %d, current is %d - no migration steps defined yet"),
            *Profile.PlayerUID, Profile.SaveVersion, CURRENT_SAVE_VERSION);
        Profile.SaveVersion = CURRENT_SAVE_VERSION;
    }
}
