#include "space/Components/JTSBossEmergenceFeedbackComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraModifier_CameraShake.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "space/Characters/JTSPlanetBoss.h"
#include "space/World/JTSPlanetAnchor.h"

void UJTSGroundTremorPattern::GetShakePatternInfoImpl(FCameraShakeInfo& Info) const
{
	Info.Duration = 3.f; Info.BlendIn = .18f; Info.BlendOut = .45f;
}
void UJTSGroundTremorPattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params) { State.Start(this, Params); }
void UJTSGroundTremorPattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& Result)
{
	const float Scale = State.Update(Params.DeltaTime) * Params.GetTotalScale();
	const float T = State.GetElapsedTime();
	Result.Location = FVector(0, 1.2f * FMath::Sin(T * 2 * PI * 9), 1.8f * FMath::Sin(T * 2 * PI * 12)) * Scale;
	Result.Rotation = FRotator(.65f * FMath::Sin(T * 2 * PI * 11), .28f * FMath::Sin(T * 2 * PI * 8), .32f * FMath::Sin(T * 2 * PI * 13)) * Scale;
	Result.Flags |= ECameraShakePatternUpdateResultFlags::SkipAutoScale;
}
UJTSGroundTremorShake::UJTSGroundTremorShake(const FObjectInitializer& Initializer) : Super(Initializer)
{
	bSingleInstance = false;
	SetRootShakePattern(CreateDefaultSubobject<UJTSGroundTremorPattern>(TEXT("Tremor")));
}
UJTSBossEmergenceFeedbackComponent::UJTSBossEmergenceFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	ShakeClass = UJTSGroundTremorShake::StaticClass();
}
void UJTSBossEmergenceFeedbackComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshAction();
}
void UJTSBossEmergenceFeedbackComponent::RefreshAction()
{
	if (GetNetMode() == NM_DedicatedServer) return;
	const auto* Boss = Cast<AJTSPlanetBoss>(GetOwner());
	if (Boss && Boss->Combat->GetAction() == EJTSBossAction::Emerge)
	{
		GetWorld()->GetTimerManager().SetTimer(FeedbackTimer, this, &ThisClass::UpdateFeedback, .05f, true);
		UpdateFeedback();
	}
	else
	{
		StopFeedback();
		GetWorld()->GetTimerManager().ClearTimer(FeedbackTimer);
	}
}
void UJTSBossEmergenceFeedbackComponent::UpdateFeedback()
{
	const auto* Boss = Cast<AJTSPlanetBoss>(GetOwner());
	if (!Boss || Boss->Combat->GetAction() != EJTSBossAction::Emerge)
	{
		StopFeedback(); GetWorld()->GetTimerManager().ClearTimer(FeedbackTimer); return;
	}
	const auto* Planet = Boss->Combat->GetPlanet();
	if (!Planet) return; // Wait for initial combat replication.
	if (Boss->Combat->GetActionElapsed() >= Boss->Combat->EmergenceDuration)
	{
		StopFeedback(); GetWorld()->GetTimerManager().ClearTimer(FeedbackTimer); return;
	}
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		auto* PC = It->Get();
		if (!PC || !PC->IsLocalController() || !PC->PlayerCameraManager) continue;
		APlayerCameraManager* Camera = PC->PlayerCameraManager;
		const APawn* Pawn = PC->GetPawn();
		const float Distance = Pawn ? FVector::Dist(Pawn->GetActorLocation(), Boss->GetActorLocation()) : BIG_NUMBER;
		FJTSPlanetSurfaceHit Ground;
		const bool bInside = Pawn && Distance < Radius && Planet->ProjectPointToSurface(Pawn->GetActorLocation(), Ground)
			&& FVector::Dist(Pawn->GetActorLocation(), Ground.ImpactPoint) < MaximumHeight;
		const float Scale = FMath::Clamp((Radius - Distance) / FMath::Max(1.f, Radius * .3f), 0.f, 1.f);
		auto* Existing = Shakes.Find(Camera);
		if (bInside && (!Existing || !Existing->IsValid() || Existing->Get()->IsFinished()) && ShakeClass)
		{
			FAddCameraShakeParams Params(Scale);
			Params.DurationOverride = FMath::Max(.01f, Boss->Combat->EmergenceDuration - Boss->Combat->GetActionElapsed());
			Shakes.Add(Camera, Camera->StartCameraShake(ShakeClass, Params));
		}
		else if (bInside && Existing && Existing->IsValid()) Existing->Get()->ShakeScale = Scale;
		else if (!bInside)
			if (auto* Shake = Shakes.Find(Camera)) { if (Shake->IsValid()) Camera->StopCameraShake(Shake->Get(), false); Shakes.Remove(Camera); }
	}
}
void UJTSBossEmergenceFeedbackComponent::StopFeedback()
{
	for (const auto& Pair : Shakes) if (Pair.Key.IsValid() && Pair.Value.IsValid()) Pair.Key->StopCameraShake(Pair.Value.Get(), false);
	Shakes.Reset();
}
void UJTSBossEmergenceFeedbackComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	StopFeedback(); GetWorld()->GetTimerManager().ClearTimer(FeedbackTimer); Super::EndPlay(Reason);
}
