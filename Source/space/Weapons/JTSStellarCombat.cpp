#include "space/Weapons/JTSStellarCombat.h"
#include "space/Player/JTSCharacter.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/Components/JTSStellarTargetComponent.h"
#include "Engine/World.h"
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
