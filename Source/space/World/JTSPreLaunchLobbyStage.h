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
 UPROPERTY(VisibleAnywhere) TObjectPtr<class UCameraComponent> LobbyCamera;
 UPROPERTY(EditAnywhere) TArray<FVector> PresentationOffsets;
 UPROPERTY(EditAnywhere) FRotator PresentationRotation;
};
