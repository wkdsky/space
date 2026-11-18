#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JTSShipWeaponModule.generated.h"

class UStaticMeshComponent;
class USoundBase;

/** Independently replaceable weapon. Blueprint authors the mount, gun, FX meshes and audio. */
UCLASS(Blueprintable)
class SPACE_API AJTSShipWeaponModule : public AActor
{
	GENERATED_BODY()
public:
	AJTSShipWeaponModule();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	void Aim(const FVector& WorldDirection, bool bFreeTurret);
	bool CanAimAt(const FVector& WorldLocation, bool bFreeTurret) const;
	bool HasClearShotAt(const AActor* Target) const;
	void FirePulse();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> WeaponRoot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Mount;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> YawPivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Turret;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> PitchPivot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Gun;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> Muzzle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Beam;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Impact;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<USoundBase> FireSound;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="0")) float Damage = 18;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin=".08")) float FireInterval = .24f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="100")) float Range = 180000;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float BeamRadius = 2.2f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float RecoilTravel = 6;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float TurnRate = 120;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float MinimumPitch = -8;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float MaximumPitch = 80;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="0",ClampMax="89")) float LimitedYaw = 6;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="0",ClampMax="89")) float LimitedPitch = 6;
	/** Fixed emitters use the installed ship axis; view intent never traverses this gun. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) bool bFixedForward = false;
private:
	UFUNCTION() void OnRep_Aim();
	UFUNCTION(NetMulticast, Unreliable) void MulticastPulse(FVector_NetQuantize End, bool bHit);
	UPROPERTY(ReplicatedUsing=OnRep_Aim) FRotator TargetAim;
	double NextFireTime = 0;
	float PulseLeft = 0;
	FVector LocalBeamEnd = FVector::ZeroVector;
	bool bPulseHit = false;
};
