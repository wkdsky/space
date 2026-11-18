#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JTSSpaceEncounterDirector.generated.h"
class AJTSSpaceEncounterUnit;
UCLASS(Blueprintable)
class SPACE_API AJTSSpaceEncounterDirector : public AActor
{
    GENERATED_BODY()
public:
    AJTSSpaceEncounterDirector();
    UPROPERTY(EditAnywhere) FName SystemId;
    UPROPERTY(EditAnywhere) TArray<TSubclassOf<AJTSSpaceEncounterUnit>> UnitClasses;
    UPROPERTY(EditAnywhere) int32 ActiveTeams=4;
    UPROPERTY(EditAnywhere) float SpawnRingMinimum=200000;
    UPROPERTY(EditAnywhere) float SpawnRingMaximum=280000;
    UPROPERTY(EditAnywhere) float RecycleDistance=450000;
    UPROPERTY(EditAnywhere) FName ApproachPlanetId;
    UPROPERTY(EditAnywhere) FName ApproachCoordinateId;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type R) override;
private:
    void Update();
    FTimerHandle DirectorTimer;
};
