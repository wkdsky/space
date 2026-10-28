#include "space/Systems/JTSLocalProfileSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "space/Player/JTSPlayerState.h"
void UJTSLocalProfileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection);
 Profile=Cast<UJTSLocalProfileSave>(UGameplayStatics::LoadGameFromSlot(TEXT("JTSLocalProfile"),0));
 if (!Profile) Profile=NewObject<UJTSLocalProfileSave>(this);
 if (!Profile->Appearance.IsValid()) Profile->Appearance=FJTSAvatarAppearance();
 FString Name;
 if (!Profile->DisplayName.IsEmpty() && !AJTSPlayerState::NormalizeDisplayName(Profile->DisplayName,Name)) Profile->DisplayName.Reset();
 else Profile->DisplayName=Name;
 Profile->UIScale=FMath::Clamp(Profile->UIScale,.8f,1.f);
}
bool UJTSLocalProfileSubsystem::Save()
{
 return Profile && UGameplayStatics::SaveGameToSlot(Profile,TEXT("JTSLocalProfile"),0);
}
