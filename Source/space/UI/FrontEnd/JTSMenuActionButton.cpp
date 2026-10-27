#include "space/UI/FrontEnd/JTSMenuActionButton.h"
#include "Kismet/GameplayStatics.h"
#include "space/Systems/JTSLocalProfileSubsystem.h"
TSharedRef<SWidget> UJTSMenuActionButton::RebuildWidget()
{
 OnClicked.AddUniqueDynamic(this,&UJTSMenuActionButton::Invoke);
 OnHovered.AddUniqueDynamic(this,&UJTSMenuActionButton::Hover);
 return Super::RebuildWidget();
}
void UJTSMenuActionButton::Invoke()
{
 if (GetWorld() && GetWorld()->GetGameInstance())
 {
  auto* P=GetWorld()->GetGameInstance()->GetSubsystem<UJTSLocalProfileSubsystem>()->Profile.Get();
  if (ClickSound) UGameplayStatics::PlaySound2D(this,ClickSound,P->MasterVolume*P->EffectsVolume);
 }
 OnAction.Broadcast(Action,Index);
}
void UJTSMenuActionButton::Hover()
{
 if (GetWorld() && GetWorld()->GetGameInstance())
 {
  auto* P=GetWorld()->GetGameInstance()->GetSubsystem<UJTSLocalProfileSubsystem>()->Profile.Get();
  if (FocusSound) UGameplayStatics::PlaySound2D(this,FocusSound,P->MasterVolume*P->EffectsVolume);
 }
}
