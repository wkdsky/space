#include "space/World/JTSEntryPresentationStage.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "space/Components/JTSAvatarAppearanceComponent.h"
#include "space/Systems/JTSLocalProfileSubsystem.h"

AJTSEntryPresentationStage::AJTSEntryPresentationStage()
{
 RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
 PresentationCamera=CreateDefaultSubobject<UCameraComponent>(TEXT("PresentationCamera"));
 PresentationCamera->SetupAttachment(RootComponent);
 PresentationCamera->bConstrainAspectRatio=false;
 PreviewMesh=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PreviewMesh"));
 PreviewMesh->SetupAttachment(RootComponent);
 PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 AppearanceComponent=CreateDefaultSubobject<UJTSAvatarAppearanceComponent>(TEXT("Appearance"));
}
void AJTSEntryPresentationStage::BeginPlay()
{
 Super::BeginPlay();
 ShowView(TEXT("Home"));
 if (auto* Local=GetGameInstance()->GetSubsystem<UJTSLocalProfileSubsystem>()) PreviewAppearance(Local->Profile->Appearance);
 if (AppearanceComponent->Catalog && AppearanceComponent->Catalog->PreviewIdle) PreviewMesh->PlayAnimation(AppearanceComponent->Catalog->PreviewIdle,true);
}
void AJTSEntryPresentationStage::ShowView(FName Name)
{
 if (const FJTSMenuCameraView* View=Views.Find(Name))
 {
  PresentationCamera->SetRelativeTransform(View->Transform);
  PresentationCamera->SetFieldOfView(View->FOV);
 }
}
void AJTSEntryPresentationStage::PreviewAppearance(const FJTSAvatarAppearance& A) { AppearanceComponent->Apply(A); }
