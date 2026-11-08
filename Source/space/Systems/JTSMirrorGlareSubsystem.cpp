#include "JTSMirrorGlareSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Components/SceneCaptureComponentCube.h"
#include "Engine/TextureRenderTargetCube.h"
#include "space/Components/JTSMirrorDefenseComponent.h"
#include "space/UI/JTSMirrorGlareWidget.h"

void UJTSMirrorGlareSubsystem::RegisterSource(UJTSMirrorDefenseComponent* Source) { Sources.AddUnique(Source); }
void UJTSMirrorGlareSubsystem::UnregisterSource(UJTSMirrorDefenseComponent* Source) { Sources.Remove(Source); }
bool UJTSMirrorGlareSubsystem::IsTickable() const { return GetWorld() && GetWorld()->IsGameWorld() && GetWorld()->GetNetMode() != NM_DedicatedServer && !Sources.IsEmpty(); }

void UJTSMirrorGlareSubsystem::Tick(float Dt)
{
	UpdateNearbyReflection(Dt);
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC || !PC->IsLocalController() || !PC->GetLocalPlayer()) continue;
		if (Widgets.ContainsByPredicate([&](const auto& W) { return W && W->GetOwningPlayer() == PC; })) continue;
		UMaterialInterface* Material = nullptr;
		for (auto Source : Sources) if (Source.IsValid() && Source->GlareMaterial) { Material = Source->GlareMaterial; break; }
		if (!Material) continue;
		auto* Widget = CreateWidget<UJTSMirrorGlareWidget>(PC, UJTSMirrorGlareWidget::StaticClass());
		Widget->InitializeMaterial(Material);
		Widget->AddToPlayerScreen(90);
		Widgets.Add(Widget);
	}
	for (int32 I = Widgets.Num()-1; I >= 0; --I)
		if (!Widgets[I] || !Widgets[I]->GetOwningPlayer()) { if (Widgets[I]) Widgets[I]->RemoveFromParent(); Widgets.RemoveAt(I); }
}

void UJTSMirrorGlareSubsystem::UpdateNearbyReflection(float Dt)
{
	ReflectionCountdown -= Dt;
	if (ReflectionCountdown > 0) return;
	ReflectionCountdown = .35f;
	UJTSMirrorDefenseComponent* Nearest = nullptr;
	double Best = FMath::Square(650.f);
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC || !PC->IsLocalController() || !PC->GetLocalPlayer()) continue;
		FVector Eye; FRotator Rotation; PC->GetPlayerViewPoint(Eye,Rotation);
		for (auto Source : Sources)
		{
			if (!Source.IsValid() || Source->ReflectiveMaterialSlots.IsEmpty() || Source->GetOwner()->IsHidden()) continue;
			const FVector Delta = Source->GetMirrorLocation()-Eye;
			const double Distance = Delta.SizeSquared();
			if (Distance < Best && FVector::DotProduct(Rotation.Vector(),Delta) > 0)
			{
				Best = Distance; Nearest = Source.Get();
			}
		}
	}
	if (ReflectedSource.Get() != Nearest && ReflectedSource.IsValid()) ReflectedSource->SetNearbyReflection(nullptr,false);
	ReflectedSource = Nearest;
	if (!Nearest) return;
	if (!ReflectionCapture)
	{
		ReflectionTexture = NewObject<UTextureRenderTargetCube>(this);
		ReflectionTexture->bHDR = false;
		ReflectionTexture->bAutoGenerateMips = true;
		ReflectionTexture->InitAutoFormat(128);
		ReflectionTexture->UpdateResourceImmediate();
		ReflectionCapture = NewObject<USceneCaptureComponentCube>(GetWorld());
		ReflectionCapture->bCaptureEveryFrame = false;
		ReflectionCapture->bCaptureOnMovement = false;
		ReflectionCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		ReflectionCapture->TextureTarget = ReflectionTexture;
		ReflectionCapture->MaxViewDistanceOverride = 3500;
		ReflectionCapture->ShowFlags.SetMotionBlur(false);
		ReflectionCapture->ShowFlags.SetScreenSpaceReflections(false);
		ReflectionCapture->ShowFlags.SetLumenReflections(false);
		ReflectionCapture->PostProcessSettings.bOverride_AutoExposureMethod = true;
		ReflectionCapture->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
		ReflectionCapture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
		ReflectionCapture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
		ReflectionCapture->RegisterComponentWithWorld(GetWorld());
	}
	ReflectionCapture->HiddenActors.Reset();
	for (auto Source : Sources) if (Source.IsValid()) ReflectionCapture->HiddenActors.Add(Source->GetOwner());
	ReflectionCapture->SetWorldLocation(Nearest->GetMirrorLocation() + Nearest->GetMirrorNormal()*5);
	ReflectionCapture->CaptureScene();
	Nearest->SetNearbyReflection(ReflectionTexture,true);
}

void UJTSMirrorGlareSubsystem::Deinitialize()
{
	for (const auto& W : Widgets) if (W) W->RemoveFromParent();
	Widgets.Reset(); Sources.Reset();
	if (ReflectionCapture) ReflectionCapture->DestroyComponent();
	ReflectionCapture = nullptr; ReflectionTexture = nullptr; ReflectedSource.Reset();
	Super::Deinitialize();
}
