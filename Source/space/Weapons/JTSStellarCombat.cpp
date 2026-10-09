#include "space/Weapons/JTSStellarCombat.h"
#include "space/Player/JTSCharacter.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/Components/JTSStellarTargetComponent.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"

FVector JTSStellarCombat::SurfaceUp(APawn* Pawn, FVector Position)
{
	const auto* Character = Cast<AJTSCharacter>(Pawn);
	return Character && Character->GetGameplayPlanet() ? Character->GetGameplayPlanet()->GetRadialUpVector(Position) : Pawn->GetActorUpVector();
}
FVector JTSStellarCombat::CastOrigin(APawn* Pawn)
{
	const FVector Eye = Pawn->GetPawnViewLocation();
	const FVector Raised = Eye + SurfaceUp(Pawn, Eye) * 75;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(StellarRaisedOrigin), false, Pawn);
	return Pawn->GetWorld()->LineTraceSingleByObjectType(Hit, Eye, Raised, FCollisionObjectQueryParams(ECC_WorldStatic), Params)
		? Eye : Raised;
}
FVector JTSStellarCombat::AimPoint(APawn* Pawn, FVector Direction, float Range, FHitResult* OutHit)
{
	const FVector Eye = Pawn->GetPawnViewLocation();
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(StellarAim), false, Pawn);
	const bool bHit = Pawn->GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + Direction * Range, ECC_Visibility, Params);
	if (OutHit) *OutHit = Hit;
	return bHit ? Hit.ImpactPoint : Eye + Direction * Range;
}
bool JTSStellarCombat::TraceBeam(UWorld* World, const FVector& Start, const FVector& End, float Radius,
	const FCollisionQueryParams& Params, FHitResult& OutHit)
{
	if (Radius <= 0) return World->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, Params);
	const bool bHit = World->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeSphere(Radius), Params);
	// Preserve normal sweep ordering for enemies and dynamic blockers. Only static terrain brushing
	// the beam's edge needs a precision fallback; the centre ray still respects every visibility blocker.
	if (!bHit || !OutHit.GetComponent() || OutHit.GetComponent()->GetCollisionObjectType() != ECC_WorldStatic) return bHit;
	FHitResult Precise;
	if (World->LineTraceSingleByChannel(Precise, Start, End, ECC_Visibility, Params))
	{
		const auto* Target = IsValid(Precise.GetActor()) ? Precise.GetActor()->FindComponentByClass<UJTSStellarTargetComponent>() : nullptr;
		const auto* Collider = Precise.GetComponent();
		if (Target && Target->IsAliveTarget() && Collider && Collider->Bounds.SphereRadius <= Radius * 2)
		{
			OutHit = Precise;
			return true;
		}
	}
	return bHit;
}

void JTSStellarCombat::DamageArea(APawn* Pawn, FVector Center, float Radius, float Damage, int32 Limit, TSet<AActor*>* Excluded)
{
	if (!IsValid(Pawn) || !Pawn->HasAuthority()) return;
	TArray<AActor*> Targets;
	UJTSStellarTargetComponent::QueryTargets(Pawn->GetWorld(), Center, Radius, FMath::Clamp(Limit, 1, 256), Targets);
	for (auto* Target : Targets) if ((!Excluded || !Excluded->Contains(Target))
		&& UJTSStellarTargetComponent::HasLineOfSight(Pawn->GetWorld(), Center, Target, Pawn))
	{
		UGameplayStatics::ApplyDamage(Target, Damage, Pawn->GetController(), Pawn, UDamageType::StaticClass());
		if (Excluded) Excluded->Add(Target);
	}
}
