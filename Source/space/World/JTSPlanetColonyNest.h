#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Interaction/JTSMeleeTarget.h"
#include "JTSPlanetColonyNest.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UJTSHealthComponent;
class AJTSPlanetEnemySettlement;
class AJTSPlanetAnchor;
class UJTSColonyDefenseComponent;

/** Indestructible colony shell. Blueprint supplies its mesh, entrances and settlement recipes. */
UCLASS(Blueprintable)
class SPACE_API AJTSPlanetColonyNest : public AActor, public IJTSMeleeTarget
{
	GENERATED_BODY()
public:
	AJTSPlanetColonyNest();
	virtual float TakeDamage(float Damage, const FDamageEvent& Event, AController* InstigatorController, AActor* Causer) override;
	virtual bool CanReceiveMeleeHit_Implementation(APawn* Pawn) const override;
	virtual void ReceiveMeleeHit_Implementation(APawn* Pawn, EJTSMeleeAttackType Type) override;
	virtual FText GetMeleeTargetDisplayName_Implementation() const override;
	virtual FText GetMeleeTargetPrompt_Implementation(APawn* Pawn) const override;
	virtual FVector GetMeleeTargetAnchorWorldLocation_Implementation() const override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Colony") TObjectPtr<UStaticMeshComponent> NestMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Colony") TObjectPtr<UJTSHealthComponent> Health;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Colony") TObjectPtr<UJTSColonyDefenseComponent> ColonyDefense;
	UPROPERTY(EditAnywhere, Category="Colony") TObjectPtr<UStaticMesh> ClosedMesh;
	/** Surface-to-top height in cm. Zero keeps the authored asset scale. Chosen once by the server. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Colony|Appearance", meta=(ClampMin="0")) FVector2D HeightRange = FVector2D::ZeroVector;
	UPROPERTY(ReplicatedUsing=ApplyAppearance, VisibleInstanceOnly, BlueprintReadOnly, Category="Colony|Appearance") float ChosenHeight = 0;
	UPROPERTY(EditInstanceOnly, Category="Colony") TObjectPtr<AJTSPlanetAnchor> Planet;
	UPROPERTY(EditInstanceOnly, Category="Colony") TObjectPtr<AActor> SurfaceController;
	UPROPERTY(EditInstanceOnly, Category="Colony") TObjectPtr<AJTSPlanetEnemySettlement> Workers;
	UPROPERTY(EditInstanceOnly, Category="Colony") TObjectPtr<AJTSPlanetEnemySettlement> Guards;
	virtual void OnConstruction(const FTransform& Transform) override;
private:
	UFUNCTION() void ApplyAppearance();
};
