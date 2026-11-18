#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JTSShipArmamentComponent.generated.h"

class AJTSShipWeaponModule;

USTRUCT(BlueprintType)
struct SPACE_API FJTSShipHardpoint
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere,BlueprintReadWrite) FName Id;
	UPROPERTY(EditAnywhere,BlueprintReadWrite) FTransform MountTransform;
	UPROPERTY(EditAnywhere,BlueprintReadWrite) TSubclassOf<AJTSShipWeaponModule> WeaponClass;
	UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bFullRotation = false;
	/** Optional movable assembly, configured in Blueprint (e.g. detachable cockpit). */
	UPROPERTY(EditAnywhere,BlueprintReadWrite) FName ParentComponentName;
};

/** Owns installations, flight fire intent and server-controlled landed defense. */
UCLASS(ClassGroup=(Ship),meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSShipArmamentComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UJTSShipArmamentComponent();
	virtual void BeginPlay()override;
	virtual void EndPlay(const EEndPlayReason::Type Reason)override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out)const override;
	UFUNCTION(BlueprintCallable,BlueprintAuthorityOnly) bool ReplaceWeapon(FName HardpointId,TSubclassOf<AJTSShipWeaponModule> WeaponClass);
	UFUNCTION(Server,Unreliable) void ServerSetWeaponIntent(FVector_NetQuantizeNormal ViewDirection,bool bFire);
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly) TArray<FJTSShipHardpoint> Hardpoints;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Ship|Defense",meta=(ClampMin="100")) float AutomaticTargetRange = 6000;
	UFUNCTION(BlueprintPure) TArray<AJTSShipWeaponModule*> GetModules() const;
private:
	void Update();
	bool HasDriver() const;
	void RefreshAutomaticTargets();
	void UpdateAutomaticDefense();
	UPROPERTY(Replicated) TArray<TObjectPtr<AJTSShipWeaponModule>> Modules;
	FVector AimDirection = FVector::ForwardVector;
	bool bFiring = false;
	double LastIntentTime = -1;
	double NextTargetRefreshTime = 0;
	TArray<TWeakObjectPtr<AActor>> AutomaticTargets;
	FTimerHandle UpdateTimer;
};
