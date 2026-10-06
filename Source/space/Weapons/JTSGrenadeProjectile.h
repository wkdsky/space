// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Items/JTSItemTypes.h"

#include "JTSGrenadeProjectile.generated.h"

class UMaterialInterface;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 * Replicated explosive round of the grenade launcher. The server owns flight, impact and every damage roll;
 * clients only see the replicated body and a local flash. It carries its own damage numbers so an upgrade the
 * shooter makes later never changes a grenade already in the air.
 */
UCLASS()
class SPACE_API AJTSGrenadeProjectile : public AActor
{
	GENERATED_BODY()

public:
	AJTSGrenadeProjectile();

	/** Server-only. Starts the flight; the damage figures are fixed here. */
	void Launch(const FVector& Direction, float Speed, float GravityScale, float InDirectDamage, float InBlastRadius,
		int32 InFragments, float InFragmentDamage, float InMiningWork, EJTSItemId InSourceItem, const FLinearColor& InColor);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleImpact(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);
	void Detonate(const FVector& Location);
	UFUNCTION()
	void OnRep_Color();

	UPROPERTY(VisibleAnywhere, Category = "Grenade")
	TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere, Category = "Grenade")
	TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere, Category = "Grenade")
	TObjectPtr<UProjectileMovementComponent> Movement;

	UPROPERTY(ReplicatedUsing = OnRep_Color)
	FLinearColor Color = FLinearColor(0.85f, 0.82f, 0.25f);

	float DirectDamage = 0.0f;
	float BlastRadius = 0.0f;
	int32 Fragments = 0;
	float FragmentDamage = 0.0f;
	float MiningWork = 0.0f;
	EJTSItemId SourceItem = EJTSItemId::None;
	bool bDetonated = false;
};
