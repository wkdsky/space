#include "space/Components/JTSSpacecraftInterferenceComponent.h"

#include "Math/RotationMatrix.h"
#include "Net/UnrealNetwork.h"
#include "space/Components/JTSCompressedSpaceComponent.h"
#include "space/Components/JTSSpacecraftFlightMovementComponent.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSSpaceWorldManager.h"

UJTSSpacecraftInterferenceComponent::UJTSSpacecraftInterferenceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UJTSSpacecraftInterferenceComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UJTSSpacecraftInterferenceComponent, Phase);
}

void UJTSSpacecraftInterferenceComponent::OnRep_Phase() { OnPhaseChanged.Broadcast(Phase); }

void UJTSSpacecraftInterferenceComponent::SetPhase(EJTSFlightInterferencePhase NewPhase)
{
	Phase = NewPhase;
	Elapsed = 0.0f;
	OnRep_Phase();
	GetOwner()->ForceNetUpdate();
}

bool UJTSSpacecraftInterferenceComponent::InterceptSegment(const FVector& Start, const FVector& End, FVector& OutEntry)
{
	auto* Ship = Cast<AJTSSpacecraftActor>(GetOwner());
	const auto* Manager = AJTSSpaceWorldManager::FindSpaceWorldManager(this);
	if (!Ship || !Ship->HasAuthority() || IsControllingFlight() || !Manager || !Manager->CompressedSpace
		|| Ship->GetFlightState() != EJTSSpacecraftFlightState::Flying) return false;
	const auto* Space = Manager->CompressedSpace.Get();
	const FVector Delta = End - Start;
	const double LengthSquared = Delta.SizeSquared();
	const FJTSCompressedCelestialBody* First = nullptr;
	double FirstTime = 2.0;
	FVector FirstCenter = FVector::ZeroVector;
	for (const auto& Body : Space->Bodies)
	{
		if (!IsValid(Body.Actor) || !Body.Interference.bEnabled || !Space->ContainsBody(Body.Actor)) continue;
		const FVector BodyCenter = Space->AuthoredCenter(Body.Actor);
		const double Radius = Space->BubbleRadius(Body.Actor) + FMath::Max(1000.0f, Body.Interference.TriggerOffset);
		const FVector Relative = Start - BodyCenter;
		const double Along = FVector::DotProduct(Relative, Delta);
		const double Outside = Relative.SizeSquared() - Radius * Radius;
		double Time = 2.0;
		if (Outside <= 0.0)
		{
			// Allow a player to leave the shell. An inward attempt is intercepted again.
			if (Along < -UE_SMALL_NUMBER || Relative.Size() < Space->BubbleRadius(Body.Actor)
				+ FMath::Max(100.0f, Body.Interference.MinimumClearance)) Time = 0.0;
		}
		else if (LengthSquared > UE_SMALL_NUMBER && Along < 0.0)
		{
			const double Discriminant = Along * Along - LengthSquared * Outside;
			if (Discriminant > 0.0) Time = (-Along - FMath::Sqrt(Discriminant)) / LengthSquared;
		}
		if (Time >= 0.0 && Time <= 1.0 && Time < FirstTime)
		{
			First = &Body; FirstTime = Time; FirstCenter = BodyCenter;
		}
	}
	if (!First) return false;
	Settings = First->Interference;
	Settings.BrakingSeconds = FMath::Max(0.1f, Settings.BrakingSeconds);
	Settings.TurningSeconds = FMath::Max(1.0f, Settings.TurningSeconds);
	Settings.RetreatSeconds = FMath::Max(0.5f, Settings.RetreatSeconds);
	Settings.RecoverySeconds = FMath::Max(0.1f, Settings.RecoverySeconds);
	Settings.TriggerOffset = FMath::Max(1000.0f, Settings.TriggerOffset);
	Settings.MinimumClearance = FMath::Clamp(Settings.MinimumClearance, 100.0f, Settings.TriggerOffset * 0.75f);
	SourceBody = First->Actor;
	Center = FirstCenter;
	NearRadius = Space->BubbleRadius(First->Actor);
	OutEntry = Start + Delta * FirstTime;
	StageOrigin = OutEntry;
	StartRotation = Ship->GetActorQuat();
	EscapeDirection = (OutEntry - Center).GetSafeNormal();
	if (EscapeDirection.IsNearlyZero()) EscapeDirection = -Ship->GetActorForwardVector();
	// Integral of v0 * (1-t/T)^2. Bound the complete path outside the near shell.
	const double BrakeBudget = FMath::Max(0.0, FVector::Distance(OutEntry, Center)
		- NearRadius - Settings.MinimumClearance);
	BrakeTravel = (Ship->GetFlightMovementComponent()->Velocity * (Settings.BrakingSeconds / 3.0f)).GetClampedToMaxSize(BrakeBudget);
	Ship->GetFlightMovementComponent()->ClearInput();
	SetPhase(EJTSFlightInterferencePhase::Braking);
	return true;
}

bool UJTSSpacecraftInterferenceComponent::UpdateFlight(float DeltaTime, UJTSSpacecraftFlightMovementComponent* Movement)
{
	if (!IsControllingFlight() || !GetOwner()->HasAuthority() || !Movement) return false;
	auto* Ship = CastChecked<AJTSSpacecraftActor>(GetOwner());
	Movement->ClearInput();
	if (!SourceBody.IsValid() || Ship->GetFlightState() != EJTSSpacecraftFlightState::Flying)
	{
		Movement->Velocity = FVector::ZeroVector;
		SetPhase(EJTSFlightInterferencePhase::None);
		return true;
	}
	// Bounded substeps retain swept collision/rotation checks even after a long frame.
	float Remaining = FMath::Max(0.0f, DeltaTime);
	while (Remaining > UE_SMALL_NUMBER && IsControllingFlight())
	{
		const float Duration = Phase == EJTSFlightInterferencePhase::Braking ? Settings.BrakingSeconds
			: Phase == EJTSFlightInterferencePhase::Turning ? Settings.TurningSeconds
			: Phase == EJTSFlightInterferencePhase::Retreating ? Settings.RetreatSeconds : Settings.RecoverySeconds;
		const float Step = FMath::Min3(Remaining, 1.0f / 60.0f, FMath::Max(UE_SMALL_NUMBER, Duration - Elapsed));
		Elapsed = FMath::Min(Duration, Elapsed + Step);
		Remaining -= Step;
		const float T = Elapsed / Duration;
		FVector Position = StageOrigin;
		FQuat Rotation = Ship->GetActorQuat();
		if (Phase == EJTSFlightInterferencePhase::Braking)
		{
			Position += BrakeTravel * (1.0f - FMath::Pow(1.0f - T, 3.0f));
			Rotation = StartRotation;
		}
		else if (Phase == EJTSFlightInterferencePhase::Turning)
		{
			const float Ease = T*T*T*(T*(T*6.0f-15.0f)+10.0f);
			Rotation = FQuat::Slerp(StartRotation, EscapeRotation, Ease).GetNormalized();
			// Small local roll pulse, returning exactly to the captured horizon at completion.
			Rotation = Rotation * FQuat(FVector::ForwardVector, FMath::DegreesToRadians(
				FMath::Clamp(Settings.TurnBankDegrees, 0.0f, 15.0f)) * FMath::Square(FMath::Sin(PI*T)));
		}
		else if (Phase == EJTSFlightInterferencePhase::Retreating)
		{
			// Raised-cosine velocity: controlled outward glide, zero speed at both ends.
			const float Progress = T - FMath::Sin(2.0f*PI*T)/(2.0f*PI);
			const double Distance = FMath::Max(double(Settings.RetreatDistance), NearRadius
				+ Settings.TriggerOffset + 1000.0 - FVector::Distance(StageOrigin, Center));
			Position += EscapeDirection * Distance * Progress;
			Rotation = EscapeRotation;
		}
		Movement->MoveInterferenceStep(Position - Ship->GetActorLocation(), Rotation, Step);
		if (Elapsed < Duration - UE_SMALL_NUMBER) continue;
		StageOrigin = Ship->GetActorLocation();
		if (Phase == EJTSFlightInterferencePhase::Braking)
		{
			EscapeDirection = (StageOrigin - Center).GetSafeNormal();
			if (EscapeDirection.IsNearlyZero()) EscapeDirection = -Ship->GetActorForwardVector();
			FVector Up = FVector::VectorPlaneProject(StartRotation.GetUpVector(), EscapeDirection).GetSafeNormal();
			if (Up.IsNearlyZero()) Up = FVector::VectorPlaneProject(StartRotation.GetRightVector(), EscapeDirection).GetSafeNormal();
			EscapeRotation = FRotationMatrix::MakeFromXZ(EscapeDirection, Up).ToQuat();
			SetPhase(EJTSFlightInterferencePhase::Turning);
		}
		else if (Phase == EJTSFlightInterferencePhase::Turning) SetPhase(EJTSFlightInterferencePhase::Retreating);
		else if (Phase == EJTSFlightInterferencePhase::Retreating) SetPhase(EJTSFlightInterferencePhase::Recovering);
		else SetPhase(EJTSFlightInterferencePhase::None);
	}
	if (!IsControllingFlight())
	{
		Movement->Velocity = FVector::ZeroVector;
		Movement->ClearInput();
	}
	return true;
}
