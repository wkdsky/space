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
void AJTSPreLaunchLobbyStage::ShowAppearance(AActor* Target,bool bHeadView)
{
 if (!Target) return;
 if (!bShowingAppearance)
 {
  SavedCamera=LobbyCamera->GetRelativeTransform();
  SavedFOV=LobbyCamera->FieldOfView;
  bShowingAppearance=true;
 }
 const FTransform& View=bHeadView ? AppearanceHeadView : AppearanceFullView;
 LobbyCamera->SetWorldTransform(View*Target->GetActorTransform());
 LobbyCamera->SetFieldOfView(AppearanceFOV);
}
void AJTSPreLaunchLobbyStage::RestoreCamera()
{
 if (!bShowingAppearance) return;
 LobbyCamera->SetRelativeTransform(SavedCamera);
 LobbyCamera->SetFieldOfView(SavedFOV);
 bShowingAppearance=false;
}
