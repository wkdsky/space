#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JTSShipCabinCouplingComponent.generated.h"

UENUM(BlueprintType)
enum class EJTSCabinCouplingState : uint8 { Docked, Releasing, Detached, Capturing };

USTRUCT(BlueprintType)
struct SPACE_API FJTSCabinCouplingMotion
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) float SideExtension = 1;
	UPROPERTY(BlueprintReadOnly) float RearExtension = 0;
	UPROPERTY(BlueprintReadOnly) float CabinTravel = 0;
	UPROPERTY(BlueprintReadOnly) float Lock = 1;
};

USTRUCT(BlueprintType)
struct SPACE_API FJTSCabinCouplingLink
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ComponentName;
	/** Delta from the authored locked pose, in the component parent's frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector RetractedOffset = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bLockPin = false;
};

USTRUCT()
struct FJTSCabinCouplingSnapshot
{
	GENERATED_BODY()
	UPROPERTY() EJTSCabinCouplingState State = EJTSCabinCouplingState::Docked;
	UPROPERTY() double StartTime = 0;
	UPROPERTY() FTransform FreePose;
};

/** Server-owned mechanical release/capture. Blueprint supplies assemblies, travel and trigger conditions. */
UCLASS(ClassGroup=(Ship), meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSShipCabinCouplingComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UJTSShipCabinCouplingComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Ship|Cabin") bool ReleaseCabin();
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Ship|Cabin") bool CaptureCabin();
	/** Optional detached-cabin movement supplied by server gameplay, independent of the fork frame. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Ship|Cabin") bool SetDetachedCabinTransform(FTransform WorldPose);
	UFUNCTION(BlueprintPure, Category="Ship|Cabin") EJTSCabinCouplingState GetCouplingState() const { return Snapshot.State; }
	UFUNCTION(BlueprintPure, Category="Ship|Cabin") bool IsCabinDocked() const { return Snapshot.State == EJTSCabinCouplingState::Docked; }
	UFUNCTION(BlueprintPure, Category="Ship|Cabin") FTransform GetCabinWorldTransform() const;
	static FJTSCabinCouplingMotion EvaluateMotion(float Progress, bool bCapture);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ship|Cabin") FName CabinRootName;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ship|Cabin") TArray<FName> CabinComponentNames;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ship|Cabin") TArray<FJTSCabinCouplingLink> SideLinks;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ship|Cabin") FName RearPistonName;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ship|Cabin") FVector EjectionOffset;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ship|Cabin") FVector RearPistonTravel;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ship|Cabin",meta=(ClampMin="0.1")) float ReleaseDuration = 3.2f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ship|Cabin",meta=(ClampMin="0.1")) float CaptureDuration = 4.2f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ship|Cabin",meta=(ClampMin="1")) float CaptureRange = 3000;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ship|Cabin") FVector ClearanceCentre;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ship|Cabin") FVector ClearanceExtent;
private:
	struct FLinkBinding { TWeakObjectPtr<USceneComponent> Component; FVector Locked; FVector Offset; bool bLockPin = false; };
	void BindRig();
	void ApplyPose();
	bool IsPathClear(const FTransform& From, const FTransform& To) const;
	double ServerTime() const;
	UFUNCTION() void OnRep_Snapshot();
	UPROPERTY(ReplicatedUsing=OnRep_Snapshot) FJTSCabinCouplingSnapshot Snapshot;
	TWeakObjectPtr<USceneComponent> CabinRoot;
	TWeakObjectPtr<USceneComponent> RearPiston;
	FVector RearLocked = FVector::ZeroVector;
	TArray<FLinkBinding> Links;
};
