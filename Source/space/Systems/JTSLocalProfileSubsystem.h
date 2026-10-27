#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameFramework/SaveGame.h"
#include "space/UI/FrontEnd/JTSAvatarTypes.h"
#include "JTSLocalProfileSubsystem.generated.h"

UCLASS()
class SPACE_API UJTSLocalProfileSave : public USaveGame
{
 GENERATED_BODY()
public:
 UPROPERTY() FJTSAvatarAppearance Appearance;
 UPROPERTY() float MasterVolume = .8f;
 UPROPERTY() float MusicVolume = .55f;
 UPROPERTY() float EffectsVolume = .8f;
 UPROPERTY() float VoiceVolume = 1.f;
 UPROPERTY() float Sensitivity = 1.f;
 UPROPERTY() bool bInvertY = false;
 UPROPERTY() bool bReducedMotion = false;
 UPROPERTY() float UIScale = 1.f;
};

/** Client-local preferences only. Expedition saves continue to belong to ExpeditionSubsystem. */
UCLASS()
class SPACE_API UJTSLocalProfileSubsystem : public UGameInstanceSubsystem
{
 GENERATED_BODY()
public:
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 UPROPERTY(Transient, BlueprintReadOnly) TObjectPtr<UJTSLocalProfileSave> Profile;
 bool Save();
};
