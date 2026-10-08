#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Items/JTSStellarWeaponCatalog.h"
#include "JTSStellarEffectActor.generated.h"

class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UAudioComponent;
class USoundBase;

/** One local reusable visual per weapon channel. Blueprint supplies all presentation assets. */
UCLASS(Blueprintable)
class SPACE_API AJTSStellarEffectActor : public AActor
{
	GENERATED_BODY()
public:
	AJTSStellarEffectActor();
	virtual void Tick(float DeltaSeconds) override;
	UFUNCTION(BlueprintCallable, Category="Stellar|Presentation") void UpdateEffect(EJTSStellarWeaponMode Mode, FVector Start, FVector End, float Radius,
		FLinearColor Color, bool bPrimary, bool bSecondary, float SecondaryRadius = 300.0f,
		bool bFromMuzzle = true, bool bImpact = false);
	/** The replicated field owns this local visual's lifetime; no channel heartbeat is required. */
	void UpdatePersistentField(FVector Center, FVector SurfaceUp, float Radius, float CoreRadius, FLinearColor Color);
	UFUNCTION(BlueprintCallable, Category="Stellar|Presentation") void UpdateArea(EJTSStellarWeaponMode Mode, FVector Center, FVector SurfaceUp, float Radius, FLinearColor Color, bool Persistent,
		bool Shield = false, bool Burst = true, float SweepAngle = 360);
	void UpdateProjectile(EJTSStellarWeaponMode Mode, FVector Center, FLinearColor Color);
	void UpdateDrone(FVector Center, FLinearColor Color);
	void UpdateOwnerShield(float Radius, FLinearColor Color);
	UFUNCTION(BlueprintImplementableEvent, Category="Stellar|Presentation")
	void OnEffectUpdated(EJTSStellarWeaponMode Mode, FVector Start, FVector End, float Radius,
		FLinearColor Color, bool bPrimary, bool bSecondary);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Beam;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Field;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Repulsion;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> BeamGlow;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Impact;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Core;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Orbit;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> OrbitInner;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<TObjectPtr<UStaticMeshComponent>> ChainBeams;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UInstancedStaticMeshComponent> Hailstones;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UInstancedStaticMeshComponent> HailTrails;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UInstancedStaticMeshComponent> HailImpacts;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stellar|Hail", meta=(ClampMin="300")) float HailHeight = 900.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stellar|Hail", meta=(ClampMin="300")) float HailFallSpeed = 1400.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stellar|Hail", meta=(ClampMin="8", ClampMax="48")) int32 HailCount = 24;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UAudioComponent> ChannelAudio;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stellar|Audio") TObjectPtr<USoundBase> LoopSound;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stellar|Audio") TObjectPtr<USoundBase> ShotSound;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stellar|Audio") TObjectPtr<USoundBase> RepulsionSound;
	/** Blueprint supplies a translucent hemisphere aligned to the planet surface. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stellar|Presentation") bool bShowRepulsionBoundary = true;
private:
	void SetBeamTransform(UStaticMeshComponent* Mesh, FVector Start, FVector End, float Width, bool bCone);
	void UpdateHail(float Time, const FQuat& Rotation);
	TArray<int64> HailCycles;
	TArray<FVector> HailLandingPositions;
	EJTSStellarWeaponMode ActiveMode = EJTSStellarWeaponMode::Jet;
	FVector Source = FVector::ZeroVector;
	FVector Destination = FVector::ZeroVector;
	FVector FieldUp = FVector::UpVector;
	float FieldRadius = 0.0f;
	float CurrentCoreRadius = 65.0f;
	float CurrentSecondaryRadius = 300.0f;
	int32 ChainIndex = 0;
	double LastMainUpdate = 0;
	double LastRepulsionSound = -1;
	bool bAreaVisual = false, bFollowOwner = false, bDroneVisual = false, bBurstVisual = false, bShieldVisual = false;
	bool bOwnerShieldVisual = false;
	float SweepDegrees = 360;
};
