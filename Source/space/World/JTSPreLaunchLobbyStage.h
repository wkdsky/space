#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JTSPreLaunchLobbyStage.generated.h"

/** Fixed camera and spawn pads; scene geometry belongs to the map. */
UCLASS()
class SPACE_API AJTSPreLaunchLobbyStage : public AActor
{
 GENERATED_BODY()
public:
 AJTSPreLaunchLobbyStage();
 UFUNCTION(BlueprintPure) FVector GetPresentationPosition(int32 SlotIndex) const;
 /** Local presentation camera only; offsets are relative to the selected crew member. */
 void ShowAppearance(AActor* Target,bool bHeadView);
 void RestoreCamera();
 UPROPERTY(EditAnywhere, Category="Appearance") FTransform AppearanceFullView;
 UPROPERTY(EditAnywhere, Category="Appearance") FTransform AppearanceHeadView;
 UPROPERTY(EditAnywhere, Category="Appearance") float AppearanceFOV=50.f;
 UPROPERTY(VisibleAnywhere) TObjectPtr<class UCameraComponent> LobbyCamera;
 UPROPERTY(EditAnywhere) TArray<FVector> PresentationOffsets;
 UPROPERTY(EditAnywhere) FRotator PresentationRotation;
private:
 bool bShowingAppearance=false;
 FTransform SavedCamera;
 float SavedFOV=50.f;
};
