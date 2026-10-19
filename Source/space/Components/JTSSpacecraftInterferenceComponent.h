#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JTSSpacecraftInterferenceComponent.generated.h"

class UJTSSpacecraftFlightMovementComponent;

UENUM(BlueprintType)
enum class EJTSFlightInterferencePhase : uint8
{
	None,
	Braking,
	Turning,
	Retreating,
	Recovering
};

/** Opt-in, level-owned exclusion narrative. Distances are physical world centimetres. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSFlightInterferenceSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEnabled = false;
	/** Intercept this far BEFORE the existing near-flight boundary. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1000", Units="cm")) float TriggerOffset = 18000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="100", Units="cm")) float MinimumClearance = 3000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1", Units="s")) float BrakingSeconds = 0.65f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1", Units="s")) float TurningSeconds = 2.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.5", Units="s")) float RetreatSeconds = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1000", Units="cm")) float RetreatDistance = 16000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1", Units="s")) float RecoverySeconds = 0.55f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="15", Units="deg")) float TurnBankDegrees = 7.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FJTSFlightInterferenceChanged, EJTSFlightInterferencePhase, Phase);

/** No independent Tick: the authoritative movement step invokes this bounded sequence. */
UCLASS(ClassGroup=(Space), meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSSpacecraftInterferenceComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UJTSSpacecraftInterferenceComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UFUNCTION(BlueprintPure, Category="Flight|Interference") bool IsControllingFlight() const { return Phase != EJTSFlightInterferencePhase::None; }
	UFUNCTION(BlueprintPure, Category="Flight|Interference") EJTSFlightInterferencePhase GetPhase() const { return Phase; }
	UPROPERTY(BlueprintAssignable, Category="Flight|Interference") FJTSFlightInterferenceChanged OnPhaseChanged;

	/** Swept sphere entry, tested before ordinary near-flight clipping, including long frames. */
	bool InterceptSegment(const FVector& Start, const FVector& End, FVector& OutEntry);
	/** Returns true for the entire frame that was owned by the sequence, including release. */
	bool UpdateFlight(float DeltaTime, UJTSSpacecraftFlightMovementComponent* Movement);
private:
	void SetPhase(EJTSFlightInterferencePhase NewPhase);
	UFUNCTION() void OnRep_Phase();
	UPROPERTY(ReplicatedUsing=OnRep_Phase, VisibleInstanceOnly, BlueprintReadOnly, Category="Flight|Interference", meta=(AllowPrivateAccess="true"))
	EJTSFlightInterferencePhase Phase = EJTSFlightInterferencePhase::None;
	TWeakObjectPtr<AActor> SourceBody;
	FJTSFlightInterferenceSettings Settings;
	FVector Center = FVector::ZeroVector;
	FVector StageOrigin = FVector::ZeroVector;
	FVector BrakeTravel = FVector::ZeroVector;
	FVector EscapeDirection = FVector::ForwardVector;
	FQuat StartRotation = FQuat::Identity;
	FQuat EscapeRotation = FQuat::Identity;
	double NearRadius = 0.0;
	float Elapsed = 0.0f;
};
