#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "space/Ships/JTSShipDockDefinition.h"
#include "JTSShipDockComponent.generated.h"

class AJTSShipDockActor;

/** Server-owned module inventory. Flight state only signals the independent dock mechanisms. */
UCLASS(ClassGroup=(Ship), meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSShipDockComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UJTSShipDockComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Function) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	/** Authoritative progression code installs by stable ID, independently of catalog/installation order. */
	UFUNCTION(BlueprintCallable) bool InstallDock(FName ModuleId);
	UFUNCTION(BlueprintCallable) bool RemoveDock(FName ModuleId);
	UFUNCTION(BlueprintPure) bool CanUseService(EJTSShipDockService Service, APawn* Pawn) const;
	/** Fire-and-forget signal: never changes flight, gear, boarding, or travel timing. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Ship|Docks") void NotifyTakeoff();
	UFUNCTION(BlueprintPure, Category="Ship|Docks") bool IsParked() const;
	UFUNCTION(BlueprintPure, Category="Ship|Docks") bool AreAllStowed() const;
	UFUNCTION(BlueprintPure, Category="Ship|Terminal") AJTSShipDockActor* GetTerminal() const { return Terminal; }
	const TArray<FName>& GetInstalledIds() const { return InstalledIds; }
	void RestoreInstalledIds(const TArray<FName>& Ids);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TArray<TObjectPtr<UJTSShipDockDefinition>> Catalog;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TArray<FName> InitialInstalledIds;
	/** Fixed ship console is independent of progression's optional module loadout. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UJTSShipDockDefinition> TerminalDefinition;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float SettleDuration = 0.65f;
private:
	void SpawnDock(FName Id);
	AJTSShipDockActor* SpawnDefinition(const UJTSShipDockDefinition* Definition);
	void Persist() const;
	UPROPERTY(Replicated) TArray<FName> InstalledIds;
	UPROPERTY(Replicated) TArray<TObjectPtr<AJTSShipDockActor>> Docks;
	UPROPERTY(Replicated) TObjectPtr<AJTSShipDockActor> Terminal;
	float StableSeconds = 0;
	FTransform LastTransform;
};
