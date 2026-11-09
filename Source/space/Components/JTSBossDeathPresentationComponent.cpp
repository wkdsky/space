#include "space/Components/JTSBossDeathPresentationComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "space/Characters/JTSPlanetBoss.h"

UJTSBossDeathPresentationComponent::UJTSBossDeathPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UJTSBossDeathPresentationComponent::BeginDeath()
{
	auto* Boss = Cast<AJTSPlanetBoss>(GetOwner());
	if (!Boss || bStarted) return;
	bStarted = true;
	const float Lifetime = Boss->Combat->GetActionDuration() + HoldDuration + FadeDuration;
	if (Boss->HasAuthority()) Boss->SetLifeSpan(FMath::Max(.1f, Lifetime - Boss->Combat->GetActionElapsed()));
	if (GetNetMode() == NM_DedicatedServer) return;
	GetWorld()->GetTimerManager().SetTimer(PresentationTimer, this, &ThisClass::UpdatePresentation, 1.f / 30.f, true);
	UpdatePresentation();
}

void UJTSBossDeathPresentationComponent::UpdatePresentation()
{
	auto* Boss = Cast<AJTSPlanetBoss>(GetOwner());
	if (!Boss) return;
	const float Age = Boss->Combat->GetActionElapsed();
	const float AnimationDuration = Boss->Combat->GetActionDuration();
	if (Age >= AnimationDuration)
	{
		if (const auto* Clip = Boss->Animations.Find(EJTSBossAction::Death); Clip && *Clip)
			Boss->Mesh->SetPosition((*Clip)->GetPlayLength(), false);
		if (auto* Animation = Boss->Mesh->GetSingleNodeInstance()) Animation->SetPlaying(false);
	}
	const float FadeAge = Age - AnimationDuration - HoldDuration;
	if (FadeAge < 0) return;
	if (FadeInstances.IsEmpty())
	{
		for (int32 Index = 0; Index < Boss->Mesh->GetNumMaterials(); ++Index)
		{
			UMaterialInterface* Material = FadeMaterials.IsValidIndex(Index) ? FadeMaterials[Index].Get() : nullptr;
			if (auto* Instance = Boss->Mesh->CreateDynamicMaterialInstance(Index, Material)) FadeInstances.Add(Instance);
		}
	}
	const float Alpha = FMath::Clamp(FadeAge / FMath::Max(.1f, FadeDuration), 0.f, 1.f);
	const float Opacity = 1.f - FMath::SmoothStep(0.f, 1.f, Alpha);
	for (UMaterialInstanceDynamic* Instance : FadeInstances) Instance->SetScalarParameterValue(OpacityParameter, Opacity);
	if (Alpha >= 1)
	{
		Boss->Mesh->SetVisibility(false);
		GetWorld()->GetTimerManager().ClearTimer(PresentationTimer);
	}
}

void UJTSBossDeathPresentationComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorld()->GetTimerManager().ClearTimer(PresentationTimer);
	Super::EndPlay(Reason);
}
