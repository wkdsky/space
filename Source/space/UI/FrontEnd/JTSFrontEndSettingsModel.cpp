#include "space/UI/FrontEnd/JTSFrontEndSettingsModel.h"
#include "space/Systems/JTSLocalProfileSubsystem.h"
#include "space/Systems/JTSVoiceSubsystem.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Sound/AmbientSound.h"
#include "Components/AudioComponent.h"

void UJTSFrontEndSettingsModel::Initialize(UGameInstance* GI)
{
 Instance=GI;
 auto* V=UGameUserSettings::GetGameUserSettings();
 Resolution=V->GetScreenResolution(); WindowMode=V->GetFullscreenMode(); Quality=V->GetOverallScalabilityLevel(); bVsync=V->IsVSyncEnabled(); FPS=V->GetFrameRateLimit();
}
void UJTSFrontEndSettingsModel::Change(FName S,int32 D)
{
 auto* P=Instance->GetSubsystem<UJTSLocalProfileSubsystem>()->Profile.Get();
 auto* Voice=Instance->GetSubsystem<UJTSVoiceSubsystem>();
 if (S==TEXT("Window")) WindowMode=(WindowMode+3+D)%3;
 else if (S==TEXT("Resolution")) { const TArray<FIntPoint> R={{1280,720},{1600,900},{1920,1080},{2560,1440}}; int32 I=R.IndexOfByKey(Resolution); Resolution=R[(FMath::Max(0,I)+4+D)%4]; }
 else if (S==TEXT("Quality")) Quality=(FMath::Max(0,Quality)+4+D)%4;
 else if (S==TEXT("Vsync")) bVsync=!bVsync;
 else if (S==TEXT("FPS")) { const TArray<float> F={30,60,120,0}; FPS=F[(FMath::Max(0,F.IndexOfByKey(FPS))+4+D)%4]; }
 else if (S==TEXT("Master")) P->MasterVolume=FMath::Clamp(P->MasterVolume+.1f*D,0.f,1.f);
 else if (S==TEXT("Music")) P->MusicVolume=FMath::Clamp(P->MusicVolume+.1f*D,0.f,1.f);
 else if (S==TEXT("Effects")) P->EffectsVolume=FMath::Clamp(P->EffectsVolume+.1f*D,0.f,1.f);
 else if (S==TEXT("Voice")) { P->VoiceVolume=FMath::Clamp(P->VoiceVolume+.1f*D,0.f,1.f); Voice->SetOutputVolume(P->VoiceVolume); }
 else if (S==TEXT("Mic")) Voice->SetInputMuted(!Voice->IsInputMuted());
 else if (S==TEXT("Input")) { auto Names=Voice->GetInputDeviceNames(); if (Names.Num()) { InputDevice=(InputDevice+Names.Num()+D)%Names.Num(); Voice->SelectInputDevice(Names[InputDevice]); } }
 else if (S==TEXT("Output")) { auto Names=Voice->GetOutputDeviceNames(); if (Names.Num()) { OutputDevice=(OutputDevice+Names.Num()+D)%Names.Num(); Voice->SelectOutputDevice(Names[OutputDevice]); } }
 else if (S==TEXT("Sensitivity")) P->Sensitivity=FMath::Clamp(P->Sensitivity+.1f*D,.2f,3.f);
 else if (S==TEXT("Invert")) P->bInvertY=!P->bInvertY;
 else if (S==TEXT("Motion")) P->bReducedMotion=!P->bReducedMotion;
 else if (S==TEXT("UIScale")) P->UIScale=FMath::Clamp(P->UIScale+.1f*D,.8f,1.f);
 SaveLocal();
}
FString UJTSFrontEndSettingsModel::Label(FName S) const
{
 const auto* P=Instance->GetSubsystem<UJTSLocalProfileSubsystem>()->Profile.Get();
 auto* Voice=Instance->GetSubsystem<UJTSVoiceSubsystem>();
 if (S==TEXT("Window")) return TArray<FString>{TEXT("全屏"),TEXT("无边框"),TEXT("窗口")}[WindowMode];
 if (S==TEXT("Resolution")) return FString::Printf(TEXT("%d × %d"),Resolution.X,Resolution.Y);
 if (S==TEXT("Quality")) return Quality<0 ? TEXT("自定义") : TArray<FString>{TEXT("低"),TEXT("中"),TEXT("高"),TEXT("极高")}[FMath::Clamp(Quality,0,3)];
 if (S==TEXT("Vsync")) return bVsync ? TEXT("开启") : TEXT("关闭");
 if (S==TEXT("FPS")) return FPS==0 ? TEXT("不限") : FString::Printf(TEXT("%.0f FPS"),FPS);
 if (S==TEXT("Mic")) return Voice->IsInputMuted() ? TEXT("已静音") : TEXT("已开启");
 if (S==TEXT("Input") || S==TEXT("Output")) { auto Names=S==TEXT("Input") ? Voice->GetInputDeviceNames() : Voice->GetOutputDeviceNames(); return Names.Num() ? Names[FMath::Clamp(S==TEXT("Input") ? InputDevice : OutputDevice,0,Names.Num()-1)] : TEXT("系统默认 / 服务未连接"); }
 if (S==TEXT("Invert")) return P->bInvertY ? TEXT("开启") : TEXT("关闭");
 if (S==TEXT("Motion")) return P->bReducedMotion ? TEXT("减少动态") : TEXT("完整动态");
 float Value=0;
 if (S==TEXT("Master")) Value=P->MasterVolume;
 else if (S==TEXT("Music")) Value=P->MusicVolume;
 else if (S==TEXT("Effects")) Value=P->EffectsVolume;
 else if (S==TEXT("Voice")) Value=P->VoiceVolume;
 else if (S==TEXT("Sensitivity")) return FString::Printf(TEXT("%.1f"),P->Sensitivity);
 else if (S==TEXT("UIScale")) Value=P->UIScale;
 return FString::Printf(TEXT("%.0f%%"),Value*100);
}
void UJTSFrontEndSettingsModel::ApplyVideo()
{
 if (IsVideoPending()) return;
 auto* V=UGameUserSettings::GetGameUserSettings();
 PreviousResolution=V->GetScreenResolution(); PreviousMode=V->GetFullscreenMode(); PreviousScalability=V->ScalabilityQuality; bPreviousVsync=V->IsVSyncEnabled(); PreviousFPS=V->GetFrameRateLimit();
 V->SetScreenResolution(Resolution); V->SetFullscreenMode(static_cast<EWindowMode::Type>(WindowMode));
 if (Quality>=0) V->SetOverallScalabilityLevel(Quality);
 V->SetVSyncEnabled(bVsync); V->SetFrameRateLimit(FPS); V->ApplySettings(false);
 Deadline=FPlatformTime::Seconds()+15;
}
void UJTSFrontEndSettingsModel::KeepVideo() { UGameUserSettings::GetGameUserSettings()->ConfirmVideoMode(); UGameUserSettings::GetGameUserSettings()->SaveSettings(); Deadline=0; }
void UJTSFrontEndSettingsModel::RevertVideo()
{
 if (!IsVideoPending()) return;
 auto* V=UGameUserSettings::GetGameUserSettings();
 V->SetScreenResolution(PreviousResolution); V->SetFullscreenMode(static_cast<EWindowMode::Type>(PreviousMode)); V->ScalabilityQuality=PreviousScalability; V->SetVSyncEnabled(bPreviousVsync); V->SetFrameRateLimit(PreviousFPS); V->ApplySettings(false);
 Deadline=0; Initialize(Instance.Get());
}
float UJTSFrontEndSettingsModel::SecondsLeft() const { return FMath::Max(0.f,float(Deadline-FPlatformTime::Seconds())); }
void UJTSFrontEndSettingsModel::Tick() { if (IsVideoPending() && SecondsLeft()<=0) RevertVideo(); }
void UJTSFrontEndSettingsModel::SaveLocal()
{
 if (!Instance.IsValid() || !Instance->GetWorld() || Instance->GetWorld()->bIsTearingDown) return;
 auto* Local=Instance->GetSubsystem<UJTSLocalProfileSubsystem>(); Local->Save();
 Instance->GetSubsystem<UJTSVoiceSubsystem>()->SetOutputVolume(Local->Profile->MasterVolume*Local->Profile->VoiceVolume);
 // Presentation audio actors are tagged in the editor; no level or sound asset paths in C++.
 for (TActorIterator<AAmbientSound> It(Instance->GetWorld()); It; ++It)
  if (It->ActorHasTag(TEXT("FrontEndAudio"))) It->GetAudioComponent()->SetVolumeMultiplier(Local->Profile->MasterVolume*(It->ActorHasTag(TEXT("Music")) ? Local->Profile->MusicVolume : Local->Profile->EffectsVolume));
}
