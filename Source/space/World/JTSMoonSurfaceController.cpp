// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/World/JTSMoonSurfaceController.h"

#include "Engine/Level.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Math/RandomStream.h"
#include "space/Core/JTSGameState.h"
#include "space/Items/JTSWorldPickupActor.h"
#include "space/Items/JTSWorldPickupItemType.h"
#include "space/Player/JTSCharacter.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/World/JTSMoonCorpseActor.h"
#include "space/World/JTSMoonSurfaceGameplayData.h"
#include "space/World/JTSMoonResourceSpawner.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetSurfaceAnchor.h"
#include "space/World/JTSMoonAntNestActor.h"

namespace
{
	constexpr double SecondsPerMinute = 60.0;
}

AJTSMoonSurfaceController::AJTSMoonSurfaceController()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	MoonCorpseClass = AJTSMoonCorpseActor::StaticClass();
}

AJTSMoonSurfaceController* AJTSMoonSurfaceController::FindMoonSurfaceController(
	const UObject* WorldContextObject,
	FName RequestedPlanetId)
{
	UWorld* const World = WorldContextObject != nullptr ? WorldContextObject->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}

	for (TActorIterator<AJTSMoonSurfaceController> ControllerIt(World); ControllerIt; ++ControllerIt)
	{
		AJTSMoonSurfaceController* const Controller = *ControllerIt;
		if (IsValid(Controller)
			&& (RequestedPlanetId.IsNone() || Controller->GetPlanetId() == RequestedPlanetId))
		{
			return Controller;
		}
	}

	return nullptr;
}

FName AJTSMoonSurfaceController::GetPlanetId() const
{
	return PlanetId;
}

bool AJTSMoonSurfaceController::IsSurfaceGameplayInitialized() const
{
	return bSurfaceGameplayInitialized;
}

bool AJTSMoonSurfaceController::SupportsPlanet(const AJTSPlanetAnchor* Planet) const
{
	return IsValid(Planet)
		&& !PlanetId.IsNone()
		&& PlanetId == Planet->GetPlanetId();
}

bool AJTSMoonSurfaceController::InitializeSurfaceGameplay(const FJTSSurfaceGameplayContext& Context)
{
	if (!HasAuthority() || !Context.HasRequiredRuntimeActors() || !SupportsPlanet(Context.Planet))
	{
		UE_LOG(LogTemp, Error, TEXT("Moon surface controller %s rejected an invalid SpaceWorld context: Planet=%s Player=%s Spacecraft=%s."),
			*GetName(),
			*GetNameSafe(Context.Planet),
			*GetNameSafe(Context.Player),
			*GetNameSafe(Context.Spacecraft));
		return false;
	}

	ApplySurfaceGameplayContext(Context);
	for (AJTSCharacter* const Player : Context.Players)
	{
		RegisterSurfacePlayer(Player);
	}
	return InitializeConfiguredSurfaceGameplay();
}

void AJTSMoonSurfaceController::RegisterSurfacePlayer(AJTSCharacter* Player)
{
	if (!IsValid(Player))
	{
		return;
	}
	ActivePlayers.RemoveAll([](const TWeakObjectPtr<AJTSCharacter>& Candidate) { return !Candidate.IsValid(); });
	ActivePlayers.AddUnique(Player);
	RegisterSurfaceRuntimeActor(Player);
}

TArray<AJTSCharacter*> AJTSMoonSurfaceController::GetActivePlayers() const
{
	TArray<AJTSCharacter*> Result;
	for (const TWeakObjectPtr<AJTSCharacter>& Player : ActivePlayers)
	{
		if (Player.IsValid()) Result.Add(Player.Get());
	}
	return Result;
}

void AJTSMoonSurfaceController::ShutdownSurfaceGameplay()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExpeditionConsumptionTimerHandle);
	}

	if (HasAuthority())
	{
		ClearGeneratedMoonAntNests();
	}
	bSurfaceGameplayInitialized = false;
	bMissingSpacecraftLogged = false;
	FoodConsumptionAccumulator = 0.0;
	WaterConsumptionAccumulator = 0.0;
}

bool AJTSMoonSurfaceController::IsSurfaceGameplayReady() const
{
	return IsSurfaceGameplayInitialized();
}

void AJTSMoonSurfaceController::SetOwningPlanet(AJTSPlanetAnchor* InOwningPlanet)
{
	OwningPlanet = InOwningPlanet;
	if (IsValid(InOwningPlanet) && !InOwningPlanet->GetPlanetId().IsNone())
	{
		PlanetId = InOwningPlanet->GetPlanetId();
	}
}

const IJTSMoonSurfaceGameplaySettings* AJTSMoonSurfaceController::GetMoonSettings() const
{
	if (IsValid(ActiveMoonGameplayData))
	{
		return static_cast<const IJTSMoonSurfaceGameplaySettings*>(ActiveMoonGameplayData.Get());
	}

	return IsValid(MoonGameplayData)
		? static_cast<const IJTSMoonSurfaceGameplaySettings*>(MoonGameplayData.Get())
		: nullptr;
}

const UJTSMoonSurfaceGameplayData* AJTSMoonSurfaceController::GetMoonGameplayData() const
{
	if (IsValid(ActiveMoonGameplayData))
	{
		return ActiveMoonGameplayData.Get();
	}

	return IsValid(MoonGameplayData) ? MoonGameplayData.Get() : nullptr;
}

AJTSPlanetAnchor* AJTSMoonSurfaceController::GetOwningPlanet() const
{
	return OwningPlanet.Get();
}

bool AJTSMoonSurfaceController::IsUsingRealPlanetSurfaceGameplay() const
{
	return OwningPlanet.IsValid();
}

AJTSSpacecraftActor* AJTSMoonSurfaceController::GetSpacecraft() const
{
	if (CachedSpacecraft.IsValid())
	{
		return CachedSpacecraft.Get();
	}
	return nullptr;
}

void AJTSMoonSurfaceController::SetSurfaceSpacecraft(AJTSSpacecraftActor* InSpacecraft)
{
	CachedSpacecraft = InSpacecraft;
	RegisterSurfaceRuntimeActor(InSpacecraft);
}

void AJTSMoonSurfaceController::ApplySurfaceGameplayContext(const FJTSSurfaceGameplayContext& Context)
{
	OwningPlanet = Context.Planet;
	PlanetId = Context.Planet->GetPlanetId();
	ActiveMoonGameplayData = Cast<UJTSMoonSurfaceGameplayData>(Context.GameplayData);

	SetSurfaceSpacecraft(Context.Spacecraft);
	RegisterSurfacePlayer(Context.Player);
}

void AJTSMoonSurfaceController::RegisterSurfaceRuntimeActor(AActor* RuntimeActor)
{
	if (!IsValid(RuntimeActor))
	{
		return;
	}

	RegisteredSurfaceRuntimeActors.RemoveAll([](const TWeakObjectPtr<AActor>& Candidate)
	{
		return !Candidate.IsValid();
	});
	RegisteredSurfaceRuntimeActors.AddUnique(RuntimeActor);
	if (AJTSCharacter* const Character = Cast<AJTSCharacter>(RuntimeActor))
	{
		ActivePlayers.RemoveAll([](const TWeakObjectPtr<AJTSCharacter>& Candidate) { return !Candidate.IsValid(); });
		ActivePlayers.AddUnique(Character);
	}
}

AJTSMoonCorpseActor* AJTSMoonSurfaceController::SpawnCorpseAtPlanetSurfaceAnchor(
	AJTSPlanetSurfaceAnchor* InCorpseSurfaceAnchor)
{
	if (!HasAuthority())
	{
		return nullptr;
	}
	if (RealSurfaceMoonCorpse.IsValid())
	{
		return RealSurfaceMoonCorpse.Get();
	}

	if (!IsValid(InCorpseSurfaceAnchor))
	{
		UE_LOG(LogTemp, Warning, TEXT("Moon surface controller %s received an invalid real-surface corpse anchor."), *GetName());
		return nullptr;
	}

	FTransform SurfaceTransform;
	if (!InCorpseSurfaceAnchor->GetSurfaceTransform(SurfaceTransform))
	{
		UE_LOG(LogTemp, Warning, TEXT("Moon surface controller %s could not resolve corpse anchor %s on a real gameplay mesh."),
			*GetName(), *InCorpseSurfaceAnchor->GetName());
		return nullptr;
	}

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	TSubclassOf<AJTSMoonCorpseActor> CorpseClass = MoonCorpseClass;
	if (CorpseClass == nullptr)
	{
		CorpseClass = AJTSMoonCorpseActor::StaticClass();
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = TEXT("JTSRealSurfaceMoonCorpse");
	SpawnParameters.OverrideLevel = GetLevel();
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AJTSMoonCorpseActor* const Corpse = World->SpawnActor<AJTSMoonCorpseActor>(CorpseClass, SurfaceTransform, SpawnParameters);
	if (!IsValid(Corpse))
	{
		UE_LOG(LogTemp, Warning, TEXT("Moon surface controller %s could not spawn its real-surface corpse."), *GetName());
		return nullptr;
	}

	if (!Corpse->SnapToPlanetSurfaceAnchor(InCorpseSurfaceAnchor))
	{
		UE_LOG(LogTemp, Warning, TEXT("Moon surface controller %s failed to snap corpse %s to its real surface anchor."),
			*GetName(), *Corpse->GetName());
		Corpse->Destroy();
		return nullptr;
	}

	RealSurfaceMoonCorpse = Corpse;
	RegisterSurfaceRuntimeActor(Corpse);
	return Corpse;
}

AJTSMoonCorpseActor* AJTSMoonSurfaceController::SpawnConfiguredCorpseAtPlanetSurfaceAnchor()
{
	return SpawnCorpseAtPlanetSurfaceAnchor(CorpseSurfaceAnchor.Get());
}

bool AJTSMoonSurfaceController::OwnsSurfaceActor(const AActor* Candidate) const
{
	if (!IsValid(Candidate))
	{
		return false;
	}
	if (Candidate == this)
	{
		return true;
	}

	return RegisteredSurfaceRuntimeActors.ContainsByPredicate([Candidate](const TWeakObjectPtr<AActor>& RegisteredActor)
	{
		return RegisteredActor.Get() == Candidate;
	});
}

ULevel* AJTSMoonSurfaceController::GetSurfaceLevel() const
{
	return GetLevel();
}

FTransform AJTSMoonSurfaceController::GetSurfacePlayerSpawnTransform(const FTransform& FallbackTransform) const
{
	if (IsValid(SurfacePlayerSpawnAnchor))
	{
		return SurfacePlayerSpawnAnchor->GetActorTransform();
	}

	return bUseSurfacePlayerSpawnTransform ? SurfacePlayerSpawnTransform : FallbackTransform;
}

FTransform AJTSMoonSurfaceController::GetSurfaceSpacecraftSpawnTransform(const FTransform& FallbackTransform) const
{
	if (IsValid(SurfaceSpacecraftSpawnAnchor))
	{
		return SurfaceSpacecraftSpawnAnchor->GetActorTransform();
	}

	return bUseSurfaceSpacecraftSpawnTransform ? SurfaceSpacecraftSpawnTransform : FallbackTransform;
}

void AJTSMoonSurfaceController::BeginPlay()
{
	Super::BeginPlay();
}

void AJTSMoonSurfaceController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ShutdownSurfaceGameplay();
	CachedSpacecraft.Reset();
	ActivePlayers.Reset();
	LevelMoonCorpseLandmark.Reset();
	RealSurfaceMoonCorpse.Reset();
	CachedLevelMoonCorpseLandmarks.Reset();
	RegisteredSurfaceRuntimeActors.Reset();
	bLevelCorpseLandmarkSearchCompleted = false;
	ActiveMoonGameplayData = nullptr;

	Super::EndPlay(EndPlayReason);
}

bool AJTSMoonSurfaceController::InitializeConfiguredSurfaceGameplay()
{
	if (!HasAuthority())
	{
		return false;
	}
	if (bSurfaceGameplayInitialized)
	{
		return true;
	}

	UWorld* const World = GetWorld();
	const IJTSMoonSurfaceGameplaySettings* const MoonSettings = GetMoonSettings();
	AJTSPlanetAnchor* const Planet = GetOwningPlanet();
	AJTSSpacecraftActor* const Spacecraft = GetSpacecraft();
	if (IsValid(Spacecraft))
	{
		Spacecraft->RestorePersistentStorage();
	}
	if (World == nullptr || MoonSettings == nullptr || !IsValid(Planet) || !IsValid(Spacecraft))
	{
		UE_LOG(LogTemp, Error, TEXT("Moon surface controller %s cannot initialize Moon gameplay: Planet=%s Settings=%s Spacecraft=%s."),
			*GetName(),
			*GetNameSafe(Planet),
			MoonSettings != nullptr ? TEXT("Valid") : TEXT("Missing Data Asset"),
			*GetNameSafe(Spacecraft));
		return false;
	}

	FoodConsumptionAccumulator = 0.0;
	WaterConsumptionAccumulator = 0.0;
	bMissingSpacecraftLogged = false;
	LevelMoonCorpseLandmark.Reset();
	CachedLevelMoonCorpseLandmarks.Reset();
	bLevelCorpseLandmarkSearchCompleted = false;

	UE_LOG(
		LogTemp,
		Log,
		TEXT("JumpToSpace Moon Surface Ready: Planet=%s ResourceCount=%d SpawnRadius=%.1f MoonAntNests=%d"),
		*PlanetId.ToString(),
		FMath::Max(0, MoonSettings->GetMoonResourceSpawnSettings().TotalResourceCount),
		FMath::Max(0.0f, MoonSettings->GetMoonResourceSpawnSettings().SpawnRadius),
		MoonSettings->GetMoonAntNestCount());

	if (IsValid(CorpseSurfaceAnchor))
	{
		SpawnConfiguredCorpseAtPlanetSurfaceAnchor();
	}

	// Landmarks must exist before procedural nests and resources derive their exclusion zones.
	InitializeMoonLandmarksAndMoonAntNests();
	InitializeMoonResources();

	World->GetTimerManager().ClearTimer(ExpeditionConsumptionTimerHandle);
	const float ConsumptionInterval = MoonSettings->GetConsumptionTickInterval();
	if (ConsumptionInterval > 0.0f)
	{
		World->GetTimerManager().SetTimer(
			ExpeditionConsumptionTimerHandle,
			this,
			&AJTSMoonSurfaceController::ConsumeExpeditionSupplies,
			ConsumptionInterval,
			true,
			ConsumptionInterval);
	}

	if (AJTSGameState* const GameState = World->GetGameState<AJTSGameState>())
	{
		GameState->SetFailureReason(EJTSFailureReason::None);
		GameState->SetGameplayPhase(EJTSGameplayPhase::MoonExploration);
	}

	bSurfaceGameplayInitialized = true;
	return true;
}

void AJTSMoonSurfaceController::InitializeMoonResources()
{
	if (!HasAuthority())
	{
		return;
	}
	const IJTSMoonSurfaceGameplaySettings* const MoonSettings = GetMoonSettings();
	if (MoonSettings == nullptr)
	{
		return;
	}

	AJTSMoonResourceSpawner* const ResourceSpawner = MoonResourceSpawner.Get();
	if (!IsValid(ResourceSpawner))
	{
		UE_LOG(LogTemp, Error, TEXT("Moon surface %s has no configured MoonResourceSpawner."), *PlanetId.ToString());
		return;
	}

	RegisterSurfaceRuntimeActor(ResourceSpawner);
	ResourceSpawner->SetSurfaceGameplayController(this);
	ResourceSpawner->ApplyMoonSpawnSettings(MoonSettings->GetMoonResourceSpawnSettings());
	ResourceSpawner->SetOwningPlanet(GetOwningPlanet());
	ResourceSpawner->SetLandmarkExclusions(GetSpacecraft(), CachedLevelMoonCorpseLandmarks, GeneratedMoonAntNests);
	ResourceSpawner->GenerateResources();
}

AJTSMoonCorpseActor* AJTSMoonSurfaceController::FindLevelCorpseLandmark()
{
	if (bLevelCorpseLandmarkSearchCompleted)
	{
		return LevelMoonCorpseLandmark.Get();
	}

	bLevelCorpseLandmarkSearchCompleted = true;
	LevelMoonCorpseLandmark.Reset();
	CachedLevelMoonCorpseLandmarks.Reset();
	if (RealSurfaceMoonCorpse.IsValid())
	{
		LevelMoonCorpseLandmark = RealSurfaceMoonCorpse;
		CachedLevelMoonCorpseLandmarks.Add(RealSurfaceMoonCorpse);
		return RealSurfaceMoonCorpse.Get();
	}

	UE_LOG(LogTemp, Warning, TEXT("Moon surface %s has no configured corpse landmark; MoonAnt Nest generation is skipped."),
		*PlanetId.ToString());
	return nullptr;
}

void AJTSMoonSurfaceController::ClearGeneratedMoonAntNests()
{
	if (!HasAuthority())
	{
		return;
	}
	for (TWeakObjectPtr<AJTSMoonAntNestActor>& Nest : GeneratedMoonAntNests)
	{
		if (Nest.IsValid())
		{
			Nest->Destroy();
		}
	}
	GeneratedMoonAntNests.Reset();
}

bool AJTSMoonSurfaceController::ResolveMoonGroundLocation(
	const FVector& CandidateLocation,
	FVector& OutGroundLocation) const
{
	const IJTSMoonSurfaceGameplaySettings* const MoonSettings = GetMoonSettings();
	AJTSPlanetAnchor* const Planet = GetOwningPlanet();
	if (MoonSettings == nullptr || !IsValid(Planet))
	{
		return false;
	}

	const float StartHeight = FMath::Max(0.0f, MoonSettings->GetMoonAntGroundTraceStartHeight());
	const float TraceDistance = FMath::Max(0.0f, MoonSettings->GetMoonAntGroundTraceDistance());
	const FVector RadialUp = Planet->GetRadialUpVector(CandidateLocation);
	FJTSPlanetSurfaceHit SurfaceHit;
	if (Planet->ProbeSurfaceAlongGravity(
		CandidateLocation + RadialUp * StartHeight,
		StartHeight + TraceDistance,
		SurfaceHit)
		|| Planet->ProjectPointToSurface(CandidateLocation, SurfaceHit))
	{
		OutGroundLocation = SurfaceHit.ImpactPoint;
		return true;
	}

	return false;
}

void AJTSMoonSurfaceController::InitializeMoonLandmarksAndMoonAntNests()
{
	if (!HasAuthority())
	{
		return;
	}
	ClearGeneratedMoonAntNests();

	UWorld* const World = GetWorld();
	const IJTSMoonSurfaceGameplaySettings* const MoonSettings = GetMoonSettings();
	AJTSSpacecraftActor* const Spacecraft = GetSpacecraft();
	AJTSPlanetAnchor* const Planet = GetOwningPlanet();
	if (World == nullptr
		|| MoonSettings == nullptr
		|| !IsValid(Planet)
		|| !IsValid(Spacecraft))
	{
		return;
	}

	AJTSMoonCorpseActor* const Corpse = FindLevelCorpseLandmark();
	if (!IsValid(Corpse))
	{
		return;
	}

	const TSubclassOf<AJTSMoonAntNestActor> NestActorClass = MoonSettings->GetMoonAntNestActorClass();
	if (NestActorClass == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Moon surface %s has no MoonAnt Nest class configured."), *PlanetId.ToString());
		return;
	}

	FRandomStream RandomStream(static_cast<int32>(FPlatformTime::Cycles64() & static_cast<uint64>(MAX_uint32)));
	const int32 DesiredNestCount = MoonSettings->GetMoonAntNestCount();
	const int32 MaxNestAttempts = FMath::Max(64, DesiredNestCount * 48);
	const FBox CorpseBounds = Corpse->GetComponentsBoundingBox(true);
	const FVector CorpseBoundsExtent = CorpseBounds.IsValid ? CorpseBounds.GetExtent() : FVector::ZeroVector;
	const float CorpseMeshClearance = CorpseBoundsExtent.Size() + 50.0f;
	const float InnerNestRadius = FMath::Max(MoonSettings->GetMoonAntNestMinDistanceFromCorpse(), CorpseMeshClearance);
	const float OuterNestRadius = FMath::Max(InnerNestRadius, MoonSettings->GetMoonAntNestOuterRadiusAroundCorpse());
	const float InnerZoneMaxRadius = FMath::Max(InnerNestRadius, OuterNestRadius * 0.45f);
	const float MidZoneMinRadius = FMath::Clamp(OuterNestRadius * 0.35f, InnerNestRadius, OuterNestRadius);
	const float MidZoneMaxRadius = FMath::Max(MidZoneMinRadius, OuterNestRadius * 0.75f);
	const float OuterZoneMinRadius = FMath::Clamp(OuterNestRadius * 0.65f, InnerNestRadius, OuterNestRadius);
	const float InnerWeight = MoonSettings->GetMoonAntNestInnerWeight();
	const float MidWeight = MoonSettings->GetMoonAntNestMidWeight();
	const float OuterWeight = MoonSettings->GetMoonAntNestOuterWeight();
	const float TotalWeight = InnerWeight + MidWeight + OuterWeight;

	auto ChooseNestRadius = [&RandomStream,
		InnerNestRadius,
		OuterNestRadius,
		InnerZoneMaxRadius,
		MidZoneMinRadius,
		MidZoneMaxRadius,
		OuterZoneMinRadius,
		InnerWeight,
		MidWeight,
		TotalWeight]()
	{
		float ZoneMinRadius = InnerNestRadius;
		float ZoneMaxRadius = InnerZoneMaxRadius;
		const float Selection = TotalWeight > KINDA_SMALL_NUMBER ? RandomStream.FRandRange(0.0f, TotalWeight) : 0.0f;
		if (TotalWeight > KINDA_SMALL_NUMBER && Selection >= InnerWeight)
		{
			if (Selection < InnerWeight + MidWeight)
			{
				ZoneMinRadius = MidZoneMinRadius;
				ZoneMaxRadius = MidZoneMaxRadius;
			}
			else
			{
				ZoneMinRadius = OuterZoneMinRadius;
				ZoneMaxRadius = OuterNestRadius;
			}
		}

		return FMath::Clamp(
			RandomStream.FRandRange(ZoneMinRadius, ZoneMaxRadius) + RandomStream.FRandRange(-30.0f, 30.0f),
			InnerNestRadius,
			OuterNestRadius);
	};

	{
		const FVector ShipLocation = Spacecraft->GetActorLocation();
		const FVector CorpseLocation = Corpse->GetActorLocation();
		FJTSPlanetSurfaceFrame CorpseSurfaceFrame;
		if (!Planet->GetSurfaceFrameAt(CorpseLocation, Corpse->GetActorForwardVector(), CorpseSurfaceFrame))
		{
			UE_LOG(LogTemp, Warning, TEXT("Moon surface %s could not resolve a real-surface corpse frame for MoonAnt Nests."), *PlanetId.ToString());
			return;
		}

		TArray<FVector> AcceptedNestLocations;
		for (int32 Attempt = 0; Attempt < MaxNestAttempts && GeneratedMoonAntNests.Num() < DesiredNestCount; ++Attempt)
		{
			const float Angle = RandomStream.FRandRange(0.0f, UE_TWO_PI);
			const float CandidateRadius = ChooseNestRadius();
			const FVector CandidateTangent = (
				CorpseSurfaceFrame.Forward * FMath::Cos(Angle)
				+ CorpseSurfaceFrame.Right * FMath::Sin(Angle)).GetSafeNormal();
			FJTSPlanetSurfaceHit NestSurfaceHit;
			if (!Planet->ProjectPointToSurface(CorpseLocation + CandidateTangent * CandidateRadius, NestSurfaceHit))
			{
				continue;
			}

			const FVector CandidateLocation = NestSurfaceHit.ImpactPoint;
			const float CorpseDistance = Planet->ApproximateSurfaceArcDistance(CorpseLocation, CandidateLocation);
			if (CorpseDistance < InnerNestRadius || CorpseDistance > OuterNestRadius + 50.0f
				|| Planet->ApproximateSurfaceArcDistance(ShipLocation, CandidateLocation) < MoonSettings->GetMoonAntNestMinDistanceFromShip())
			{
				continue;
			}

			const float CandidateMinSpacing = MoonSettings->GetMoonAntNestBaseMinSpacing()
				* RandomStream.FRandRange(
					MoonSettings->GetMoonAntNestCandidateSpacingScaleMin(),
					MoonSettings->GetMoonAntNestCandidateSpacingScaleMax());
			bool bOverlapsExistingNest = false;
			for (const FVector& ExistingLocation : AcceptedNestLocations)
			{
				if (Planet->ApproximateSurfaceArcDistance(ExistingLocation, CandidateLocation) < CandidateMinSpacing)
				{
					bOverlapsExistingNest = true;
					break;
				}
			}
			if (bOverlapsExistingNest)
			{
				continue;
			}

			FJTSPlanetSurfaceFrame NestSurfaceFrame;
			if (!Planet->GetSurfaceFrameAt(CandidateLocation, CandidateTangent, NestSurfaceFrame))
			{
				continue;
			}
			const FVector NestForward = FQuat(
				NestSurfaceFrame.Up,
				RandomStream.FRandRange(0.0f, UE_TWO_PI)).RotateVector(NestSurfaceFrame.Forward);
			const FTransform NestTransform(
				FRotationMatrix::MakeFromXZ(NestForward, NestSurfaceFrame.Up).ToQuat(),
				CandidateLocation);
			AJTSMoonAntNestActor* const Nest = World->SpawnActorDeferred<AJTSMoonAntNestActor>(
				NestActorClass,
				NestTransform,
				Corpse,
				nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!IsValid(Nest))
			{
				continue;
			}

			Nest->SetMoonAntActorClass(MoonSettings->GetMoonAntActorClass());
			Nest->SetMoonAntNestVisualScale(RandomStream.FRandRange(
				MoonSettings->GetMoonAntNestVisualScaleVariationMin(),
				MoonSettings->GetMoonAntNestVisualScaleVariationMax()));
			RegisterSurfaceRuntimeActor(Nest);
			Nest->FinishSpawning(NestTransform);
			Nest->PlaceOnPlanetSurface(Planet, CandidateLocation, NestForward);
			GeneratedMoonAntNests.Add(Nest);
			AcceptedNestLocations.Add(CandidateLocation);
		}

		UE_LOG(LogTemp, Log, TEXT("JumpToSpace MoonAnt Nests: Planet=%s Requested=%d Spawned=%d"),
			*PlanetId.ToString(), DesiredNestCount, GeneratedMoonAntNests.Num());
	}
}

void AJTSMoonSurfaceController::ConsumeExpeditionSupplies()
{
	if (!HasAuthority())
	{
		return;
	}
	const IJTSMoonSurfaceGameplaySettings* const MoonSettings = GetMoonSettings();
	AJTSSpacecraftActor* const Spacecraft = GetSpacecraft();
	if (MoonSettings == nullptr || !IsValid(Spacecraft))
	{
		FoodConsumptionAccumulator = 0.0;
		WaterConsumptionAccumulator = 0.0;
		if (!bMissingSpacecraftLogged)
		{
			UE_LOG(LogTemp, Error, TEXT("Moon surface %s cannot consume expedition supplies because its spacecraft is unavailable."), *PlanetId.ToString());
			bMissingSpacecraftLogged = true;
		}
		return;
	}
	bMissingSpacecraftLogged = false;

	const double ConsumptionTickSeconds = static_cast<double>(MoonSettings->GetConsumptionTickInterval());
	const double ConsumptionUnit = static_cast<double>(MoonSettings->GetMinimumConsumptionUnit());
	if (ConsumptionTickSeconds <= 0.0 || ConsumptionUnit <= 0.0)
	{
		return;
	}

	const double CrewCount = static_cast<double>(FMath::Max(1, GetActivePlayers().Num()));
	FoodConsumptionAccumulator += static_cast<double>(MoonSettings->GetFoodConsumptionPerPersonPerMinute())
		* CrewCount * ConsumptionTickSeconds / SecondsPerMinute;
	WaterConsumptionAccumulator += static_cast<double>(MoonSettings->GetWaterConsumptionPerPersonPerMinute())
		* CrewCount * ConsumptionTickSeconds / SecondsPerMinute;

	const int32 FoodResourcesDue = GetWholeConsumptionUnits(FoodConsumptionAccumulator, ConsumptionUnit);
	const int32 WaterResourcesDue = GetWholeConsumptionUnits(WaterConsumptionAccumulator, ConsumptionUnit);
	if (FoodResourcesDue > 0)
	{
		Spacecraft->TryConsumeResource(EJTSResourceType::Food,
			FMath::Min(FoodResourcesDue, Spacecraft->GetResourceAmount(EJTSResourceType::Food)));
	}
	if (WaterResourcesDue > 0)
	{
		Spacecraft->TryConsumeResource(EJTSResourceType::Water,
			FMath::Min(WaterResourcesDue, Spacecraft->GetResourceAmount(EJTSResourceType::Water)));
	}

	FoodConsumptionAccumulator = FMath::Max(0.0, FoodConsumptionAccumulator - static_cast<double>(FoodResourcesDue) * ConsumptionUnit);
	WaterConsumptionAccumulator = FMath::Max(0.0, WaterConsumptionAccumulator - static_cast<double>(WaterResourcesDue) * ConsumptionUnit);
}

int32 AJTSMoonSurfaceController::GetWholeConsumptionUnits(double Accumulator, double MinimumConsumptionUnit)
{
	if (!FMath::IsFinite(Accumulator) || !FMath::IsFinite(MinimumConsumptionUnit)
		|| Accumulator <= 0.0 || MinimumConsumptionUnit <= 0.0)
	{
		return 0;
	}

	const double WholeUnits = FMath::FloorToDouble((Accumulator / MinimumConsumptionUnit) + 1.0e-9);
	return WholeUnits >= static_cast<double>(MAX_int32) ? MAX_int32 : static_cast<int32>(WholeUnits);
}
