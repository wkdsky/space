#pragma once
#include "CoreMinimal.h"
#include "Scalability.h"
#include "UObject/Object.h"
#include "JTSFrontEndSettingsModel.generated.h"

/** Local preferences and reversible video transaction; no widget construction. */
UCLASS()
class SPACE_API UJTSFrontEndSettingsModel : public UObject
{
 GENERATED_BODY()
public:
 void Initialize(class UGameInstance* Instance);
 void Change(FName Setting, int32 Direction);
 FString Label(FName Setting) const;
 void ApplyVideo();
 void KeepVideo();
 void RevertVideo();
 bool IsVideoPending() const { return Deadline>0; }
 float SecondsLeft() const;
 void Tick();
 void SaveLocal();
private:
 TWeakObjectPtr<UGameInstance> Instance;
 FIntPoint Resolution=FIntPoint(1920,1080), PreviousResolution;
 int32 WindowMode=1,PreviousMode=1,Quality=2;
 Scalability::FQualityLevels PreviousScalability;
 bool bVsync=true,bPreviousVsync=true;
 float FPS=60,PreviousFPS=60;
 double Deadline=0;
 int32 InputDevice=0,OutputDevice=0;
};
