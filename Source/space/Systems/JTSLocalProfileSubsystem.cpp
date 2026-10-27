#include "space/Systems/JTSLocalProfileSubsystem.h"
#include "Kismet/GameplayStatics.h"
void UJTSLocalProfileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection);
 Profile=Cast<UJTSLocalProfileSave>(UGameplayStatics::LoadGameFromSlot(TEXT("JTSLocalProfile"),0));
 if (!Profile) Profile=NewObject<UJTSLocalProfileSave>(this);
 if (!Profile->Appearance.IsValid()) Profile->Appearance=FJTSAvatarAppearance();
 Profile->UIScale=FMath::Clamp(Profile->UIScale,.8f,1.f);
}
bool UJTSLocalProfileSubsystem::Save()
{
 return Profile && UGameplayStatics::SaveGameToSlot(Profile,TEXT("JTSLocalProfile"),0);
}
