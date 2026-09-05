#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "JTSMoonWrappedActorComponent.generated.h"

class AJTSMoonWorldActor;
class UJTSMoonWrapSubsystem;
class UMaterialInterface;

/** Places one authoritative actor at the local player's nearest periodic physical image in Fake Moon worlds. */
UCLASS(ClassGroup = (Moon), meta = (BlueprintSpawnableComponent))
class SPACE_API UJTSMoonWrappedActorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UJTSMoonWrappedActorComponent();

	UFUNCTION(BlueprintPure, Category = "Moon|Wrapping")
	FVector2D GetLogicalPosition2D() const;

	UFUNCTION(BlueprintCallable, Category = "Moon|Wrapping")
	void SetLogicalPosition2D(const FVector2D& NewLogicalPosition2D);

	UFUNCTION(BlueprintCallable, Category = "Moon|Wrapping")
	void SetLogicalPositionFromWorld();

	UFUNCTION(BlueprintPure, Category = "Moon|Wrapping")
	bool IsMoonWrappingEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "Moon|Wrapping")
	void RefreshPhysicalImage();

	/** Sets the optional shared material used by visible primitive components on the owner. */
	UFUNCTION(BlueprintCallable, Category = "Moon|Rendering")
	void SetFakeMoonBendMaterial(UMaterialInterface* NewMaterial);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void ResolveMoonWorld();
	void ApplyBendRenderingSettings();
	bool IsOwnerLocalControlled() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Wrapping", meta = (AllowPrivateAccess = "true"))
	FVector2D LogicalPosition2D = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Wrapping", meta = (AllowPrivateAccess = "true"))
	bool bUseActorLocationAsInitialLogicalPosition = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Wrapping|Debug", meta = (AllowPrivateAccess = "true"))
	bool bEnableDebugLogging = false;

	/** Optional shared material containing the JTSFakeMoon WPO function. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Rendering", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> FakeMoonBendMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon|Rendering", meta = (AllowPrivateAccess = "true"))
	bool bApplyBendBoundsScale = true;

	TWeakObjectPtr<AJTSMoonWorldActor> FakeWorld;
	TWeakObjectPtr<UJTSMoonWrapSubsystem> WrapSubsystem;
	bool bMoonWrappingEnabled = false;
	bool bHasPhysicalImage = false;
	bool bRenderingSettingsApplied = false;
};
