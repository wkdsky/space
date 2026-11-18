#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/Interaction/IInteractable.h"
#include "space/Ships/JTSShipDockDefinition.h"
#include "JTSShipDockActor.generated.h"

class AJTSSpacecraftActor;
class UStaticMeshComponent;
class UBoxComponent;
class AJTSPortableCredentialDock;

/** One replicated command prevents partially updated start time/direction on clients. */
USTRUCT()
struct FJTSShipDockPose
{
	GENERATED_BODY()
	UPROPERTY() bool bDeployed = false;
	UPROPERTY() float StartAlpha = 0;
	UPROPERTY() double ServerStartTime = 0;
};

/** Independently replicated, replaceable ship plugin. Blueprint supplies every visual asset. */
UCLASS(Blueprintable)
class SPACE_API AJTSShipDockActor : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	AJTSShipDockActor();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	virtual bool CanRemoveDock() const { return true; }
	void InitializeDock(AJTSSpacecraftActor* Ship);
	void SetDeployed(bool bDeploy);
	UFUNCTION(BlueprintPure) bool IsDeployed() const;
	UFUNCTION(BlueprintPure) bool IsStowed() const;
	UFUNCTION(BlueprintPure) float GetDeploymentAlpha() const;
	UFUNCTION(BlueprintPure) AJTSSpacecraftActor* GetSpacecraft() const;
	UFUNCTION(BlueprintPure) FVector GetScreenWorldLocation() const;
	/** Closest visible device surface; walking up to a tray or housing also permits interaction. */
	UFUNCTION(BlueprintPure) virtual FVector GetInteractionWorldLocation(const FVector& ReferenceLocation) const;
	UFUNCTION(BlueprintPure) bool CanUseDock(const APawn* Pawn) const;
	/** Shared dock readiness, with LOS to the exposed device being used. */
	bool CanUseDockAt(const APawn* Pawn, const FVector& InteractionLocation) const;
	EJTSShipDockService GetService() const { return Service; }
	virtual bool CanInteract_Implementation(APawn* Pawn) const override;
	virtual FText GetInteractionPrompt_Implementation(APawn* Pawn) const override;
	virtual void Interact_Implementation(APawn* Pawn) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> DockRoot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Mount;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> UpperBayDoor;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> LowerBayDoor;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> LeftStrut;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> RightStrut;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> Carriage;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> ScreenHinge;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Screen;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> TrayHinge;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Tray;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> ScreenAnchor;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> ScreenTarget;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) EJTSShipDockService Service = EJTSShipDockService::Custom;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FText DisplayName;
	UPROPERTY(EditDefaultsOnly, Category="Portable Device") TSubclassOf<AJTSPortableCredentialDock> CredentialDockClass;
	UPROPERTY(EditDefaultsOnly, Category="Portable Device") FTransform CredentialDockTransform;
	/** Built-in consoles share screen interaction, but never take part in the module mechanism. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) bool bDeployable = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FVector StowedOffset = FVector(0, -150, 15);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FVector ClearanceOffset = FVector(0, 65, 15);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FVector DeployedOffset = FVector(0, 35, -110);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FVector FoldedScreenLocation = FVector(0, 65, 20);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FVector FoldedTrayLocation = FVector(0, 46, -20);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float BayDoorTravel = 54;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float DeploymentDuration = 3.2f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float RetractionDuration = 2.8f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float InteractionDistance = 185;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float StrutSpacing = 53;
	/** Custom plugins may implement their own service without modifying the manager. */
	UFUNCTION(BlueprintImplementableEvent) void OnDockUsed(APawn* Pawn);
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
	UFUNCTION() void OnRep_Pose();
	void ApplyPose();
	double ServerTime() const;
	UPROPERTY(ReplicatedUsing=OnRep_Pose) FJTSShipDockPose Pose;
	UPROPERTY(Replicated) TObjectPtr<AJTSPortableCredentialDock> CredentialDock;
};
