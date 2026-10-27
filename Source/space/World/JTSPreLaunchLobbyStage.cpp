#include "space/World/JTSPreLaunchLobbyStage.h"
#include "Camera/CameraComponent.h"
AJTSPreLaunchLobbyStage::AJTSPreLaunchLobbyStage()
{
 RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
 LobbyCamera=CreateDefaultSubobject<UCameraComponent>(TEXT("LobbyCamera"));
 LobbyCamera->SetupAttachment(RootComponent);
 LobbyCamera->bConstrainAspectRatio=false;
}
FVector AJTSPreLaunchLobbyStage::GetPresentationPosition(int32 Index) const
{
 return GetActorTransform().TransformPosition(PresentationOffsets.IsValidIndex(Index) ? PresentationOffsets[Index] : FVector::ZeroVector);
}
