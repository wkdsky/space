// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/World/JTSPlanetLandingManager.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Math/RotationMatrix.h"
#include "space/Components/JTSSpacecraftGroundProbeComponent.h"
#include "space/Core/JTSGameInstance.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetArrivalAnchor.h"
#include "space/World/JTSPlanetLandingSite.h"
#include "space/World/JTSSpaceWorldManager.h"

namespace
{
	TMap<const UWorld*, TWeakObjectPtr<AJTSPlanetLandingManager>> GPlanetLandingManagers;

	const TCHAR* GetLandingValidationFailureName(EJTSLandingValidationFailure Failure)
	{
		switch (Failure)
		{
		case EJTSLandingValidationFailure::None:
			return TEXT("None");
		case EJTSLandingValidationFailure::NoPlanet:
			return TEXT("NoPlanet");
		case EJTSLandingValidationFailure::NoLandingSite:
			return TEXT("NoLandingSite");
		case EJTSLandingValidationFailure::OutsideLandingArea:
			return TEXT("OutsideLandingArea");
		case EJTSLandingValidationFailure::TooHigh:
			return TEXT("TooHigh");
		case EJTSLandingValidationFailure::TooFast:
			return TEXT("TooFast");
		case EJTSLandingValidationFailure::InvalidAttitude:
			return TEXT("InvalidAttitude");
		case EJTSLandingValidationFailure::TooSteep:
			return TEXT("TooSteep");
		case EJTSLandingValidationFailure::NoSurface:
			return TEXT("NoSurface");
		case EJTSLandingValidationFailure::CollisionBlocked:
			return TEXT("CollisionBlocked");
		case EJTSLandingValidationFailure::InvalidLandingTarget:
			return TEXT("InvalidLandingTarget");
		case EJTSLandingValidationFailure::InvalidFlightState:
			return TEXT("InvalidFlightState");
		default:
			return TEXT("Unknown");
		}
	}

	FVector MakeTangentForward(const FVector& PreferredForward, const FVector& Up)
	{
		FVector Forward = FVector::VectorPlaneProject(PreferredForward, Up).GetSafeNormal();
		if (!Forward.IsNearlyZero())
		{
			return Forward;
		}

		FVector TangentX;
		FVector TangentY;
		Up.FindBestAxisVectors(TangentX, TangentY);
		return TangentX.GetSafeNormal();
	}
}

DEFINE_LOG_CATEGORY_STATIC(LogJTSPlanetLanding, Log, All);

AJTSPlanetLandingManager::AJTSPlanetLandingManager()
{
	PrimaryActorTick.bCanEverTick = false;
	DefaultSpacecraftClass = AJTSSpacecraftActor::StaticClass();
}

AJTSPlanetLandingManager* AJTSPlanetLandingManager::FindPlanetLandingManager(const UObject* WorldContextObject)
{
	UWorld* const World = WorldContextObject != nullptr ? WorldContextObject->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}

	if (const TWeakObjectPtr<AJTSPlanetLandingManager>* const CachedManager = GPlanetLandingManagers.Find(World))
	{
		if (AJTSPlanetLandingManager* const Manager = CachedManager->Get(); IsValid(Manager))
		{
			return Manager;
		}

		GPlanetLandingManagers.Remove(World);
	}

	if (World->PersistentLevel == nullptr)
	{
		return nullptr;
	}

	for (AActor* const Actor : World->PersistentLevel->Actors)
	{
		if (AJTSPlanetLandingManager* const Manager = Cast<AJTSPlanetLandingManager>(Actor); IsValid(Manager))
		{
			GPlanetLandingManagers.Add(World, Manager);
			return Manager;
		}
	}

	return nullptr;
}

void AJTSPlanetLandingManager::BeginPlay()
{
	Super::BeginPlay();

	GPlanetLandingManagers.Add(GetWorld(), this);
	DiscoverPersistentLandingSites();
}

void AJTSPlanetLandingManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const TWeakObjectPtr<AJTSPlanetLandingManager>* const CachedManager = GPlanetLandingManagers.Find(GetWorld());
		CachedManager != nullptr && CachedManager->Get() == this)
	{
		GPlanetLandingManagers.Remove(GetWorld());
	}

	RegisteredLandingSites.Empty();
	ArrivalSpacecraftByPlanet.Empty();
	PlayerSpacecraft.Empty();

	Super::EndPlay(EndPlayReason);
}

bool AJTSPlanetLandingManager::RegisterLandingSite(AJTSPlanetLandingSite* LandingSite)
{
	if (!IsValid(LandingSite))
	{
		return false;
	}

	RegisteredLandingSites.Add(LandingSite);
	UE_LOG(LogJTSPlanetLanding, Log, TEXT("LandingSite Registered: Site=%s Planet=%s"),
		*GetNameSafe(LandingSite), *LandingSite->GetPlanetId().ToString());
	return true;
}

void AJTSPlanetLandingManager::UnregisterLandingSite(AJTSPlanetLandingSite* LandingSite)
{
	if (LandingSite != nullptr)
	{
		RegisteredLandingSites.Remove(LandingSite);
	}
}

TArray<AJTSPlanetLandingSite*> AJTSPlanetLandingManager::GetLandingSitesForPlanet(AJTSPlanetAnchor* Planet) const
{
	TArray<AJTSPlanetLandingSite*> Result;
	if (!IsValid(Planet))
	{
		return Result;
	}

	for (const TWeakObjectPtr<AJTSPlanetLandingSite>& WeakLandingSite : RegisteredLandingSites)
	{
		AJTSPlanetLandingSite* const LandingSite = WeakLandingSite.Get();
		if (IsValid(LandingSite) && LandingSite->GetPlanetAnchor() == Planet)
		{
			Result.Add(LandingSite);
		}
	}

	Result.Sort([](const AJTSPlanetLandingSite& Left, const AJTSPlanetLandingSite& Right)
	{
		return Left.GetPathName() < Right.GetPathName();
	});
	return Result;
}

bool AJTSPlanetLandingManager::IsLocationInsideLandingArea(AJTSPlanetAnchor* Planet, const FVector& Location) const
{
	for (AJTSPlanetLandingSite* const LandingSite : GetLandingSitesForPlanet(Planet))
	{
		if (LandingSite->IsLandingEnabled() && LandingSite->IsLocationInsideLandingArea(Location))
		{
			return true;
		}
	}

	return false;
}

AJTSPlanetLandingSite* AJTSPlanetLandingManager::GetNearestLandingSite(
	AJTSPlanetAnchor* Planet,
	const FVector& Location) const
{
	AJTSPlanetLandingSite* NearestSite = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (AJTSPlanetLandingSite* const LandingSite : GetLandingSitesForPlanet(Planet))
	{
		if (!IsValid(LandingSite) || !LandingSite->IsLandingEnabled())
		{
			continue;
		}

		FVector ClosestAreaPoint;
		if (!LandingSite->FindClosestPointInLandingArea(Location, ClosestAreaPoint))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(Location, ClosestAreaPoint);
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestSite = LandingSite;
		}
	}

	return NearestSite;
}

float AJTSPlanetLandingManager::GetLandingDistance(AJTSPlanetAnchor* Planet, const FVector& Location) const
{
	AJTSPlanetLandingSite* const NearestSite = GetNearestLandingSite(Planet, Location);
	if (!IsValid(NearestSite))
	{
		return -1.0f;
	}

	FVector ClosestAreaPoint;
	return NearestSite->FindClosestPointInLandingArea(Location, ClosestAreaPoint)
		? FVector::Distance(Location, ClosestAreaPoint)
		: -1.0f;
}

bool AJTSPlanetLandingManager::IsLandingAvailable(AJTSSpacecraftActor* Spacecraft) const
{
	FJTSPlanetLandingValidationResult ValidationResult;
	AJTSPlanetAnchor* Planet = nullptr;
	return QueryLandingAvailability(Spacecraft, ValidationResult, Planet) && ValidationResult.bIsValid;
}

void AJTSPlanetLandingManager::DrawDebugLandingAreas(AJTSPlanetAnchor* Planet, float Duration) const
{
	for (AJTSPlanetLandingSite* const LandingSite : GetLandingSitesForPlanet(Planet))
	{
		if (IsValid(LandingSite) && LandingSite->IsLandingEnabled())
		{
			LandingSite->DrawDebugLandingSite(Duration);
		}
	}
}

bool AJTSPlanetLandingManager::StartLandingSequence(APlayerController* PlayerController, AJTSPlanetAnchor* Planet)
{
	if (!IsValid(PlayerController) || !IsValid(Planet))
	{
		UE_LOG(LogJTSPlanetLanding, Error, TEXT("Landing Start failed: PlayerController or Planet is invalid."));
		return false;
	}

	UE_LOG(LogJTSPlanetLanding, Log, TEXT("Landing Start: Planet=%s PlayerController=%s"),
		*Planet->GetPlanetId().ToString(), *GetNameSafe(PlayerController));

	if (AJTSSpaceWorldManager* const SpaceWorldManager = AJTSSpaceWorldManager::FindSpaceWorldManager(this))
	{
		SpaceWorldManager->SetCurrentPlanet(Planet);
		SpaceWorldManager->SetTravelState(EJTSSpaceTravelState::Surface);
		SpaceWorldManager->RequestPlanetContentLoad(Planet, true);
	}

	FTransform PlayerSpawnTransform;
	FTransform SpacecraftSpawnTransform;
	if (!ResolveArrivalTransforms(PlayerController, Planet, PlayerSpawnTransform, SpacecraftSpawnTransform))
	{
		UE_LOG(LogJTSPlanetLanding, Error, TEXT("Landing Start failed: Planet=%s could not resolve independent arrival transforms."),
			*Planet->GetPlanetId().ToString());
		return false;
	}

	AJTSCharacter* Character = nullptr;
	const bool bCharacterSpawned = SpawnAndConfigureCharacter(PlayerController, Planet, PlayerSpawnTransform, Character);
	UE_LOG(LogJTSPlanetLanding, Log, TEXT("Character Spawn Success: %s Planet=%s Spawn Location=%s Character=%s"),
		bCharacterSpawned ? TEXT("TRUE") : TEXT("FALSE"),
		*Planet->GetPlanetId().ToString(),
		*PlayerSpawnTransform.GetLocation().ToCompactString(),
		*GetNameSafe(Character));
	UE_LOG(LogJTSPlanetLanding, Log, TEXT("Gravity Enabled: %s Planet=%s Character=%s"),
		bCharacterSpawned && Character->IsPlanetGravityEnabled() ? TEXT("TRUE") : TEXT("FALSE"),
		*Planet->GetPlanetId().ToString(),
		*GetNameSafe(Character));

	AJTSSpacecraftActor* const Spacecraft = FindOrSpawnArrivalSpacecraft(
		Planet,
		SpacecraftSpawnTransform,
		Character != nullptr ? Character->GetActorLocation() : PlayerSpawnTransform.GetLocation());
	const bool bSpacecraftSpawned = IsValid(Spacecraft);
	UE_LOG(LogJTSPlanetLanding, Log, TEXT("Spacecraft Spawn Success: %s Planet=%s Requested Location=%s Final Location=%s Grounded=%s Spacecraft=%s"),
		bSpacecraftSpawned ? TEXT("TRUE") : TEXT("FALSE"),
		*Planet->GetPlanetId().ToString(),
		*SpacecraftSpawnTransform.GetLocation().ToCompactString(),
		bSpacecraftSpawned ? *Spacecraft->GetActorLocation().ToCompactString() : TEXT("<none>"),
		bSpacecraftSpawned && Spacecraft->IsGroundedOnPlanet() ? TEXT("TRUE") : TEXT("FALSE"),
		*GetNameSafe(Spacecraft));
	if (bSpacecraftSpawned && Spacecraft->IsGroundedOnPlanet())
	{
		UE_LOG(LogJTSPlanetLanding, Log, TEXT("Ground Detection Result: Context=InitialSpacecraftSpawn Planet=%s Hit=TRUE Source=InitialParking Location=%s"),
			*Planet->GetPlanetId().ToString(),
			*Spacecraft->GetActorLocation().ToCompactString());
	}
	else
	{
		LogGroundDetectionResult(
			Planet,
			bSpacecraftSpawned ? Spacecraft->GetActorLocation() : SpacecraftSpawnTransform.GetLocation(),
			TEXT("InitialSpacecraftSpawn"));
	}

	if (bSpacecraftSpawned)
	{
		RegisterPlayerSpacecraft(PlayerController, Spacecraft);
	}

	if (bCharacterSpawned)
	{
		if (AJTSSpaceWorldManager* const SpaceWorldManager = AJTSSpaceWorldManager::FindSpaceWorldManager(this))
		{
			SpaceWorldManager->SetSurfaceGameplayReady(true);
		}
		if (AJTSPlayerController* const JTSPlayerController = Cast<AJTSPlayerController>(PlayerController))
		{
			JTSPlayerController->ApplySpaceWorldInputMode();
		}
	}

	// The two spawns are intentionally independent, but the arrival sequence is complete only once
	// both runtime actors exist. A partial result remains observable in logs and can be retried.
	return bCharacterSpawned && bSpacecraftSpawned;
}

bool AJTSPlanetLandingManager::RequestLanding(
	AJTSSpacecraftActor* Spacecraft,
	FJTSPlanetLandingValidationResult& OutResult)
{
	OutResult = FJTSPlanetLandingValidationResult();
	if (!IsValid(Spacecraft))
	{
		OutResult.Failure = EJTSLandingValidationFailure::InvalidFlightState;
		return false;
	}

	if (Spacecraft->GetFlightState() != EJTSSpacecraftFlightState::LandingRequest)
	{
		OutResult.Failure = EJTSLandingValidationFailure::InvalidFlightState;
		Spacecraft->CancelLandingRequest(OutResult.Failure);
		return false;
	}

	AJTSPlanetAnchor* Planet = nullptr;
	if (QueryLandingAvailability(Spacecraft, OutResult, Planet) && OutResult.bIsValid)
	{
		if (Spacecraft->BeginLandingAssist(OutResult, LandingAssistDuration))
		{
			if (AJTSSpaceWorldManager* const SpaceWorldManager = AJTSSpaceWorldManager::FindSpaceWorldManager(this))
			{
				if (SpaceWorldManager->GetCurrentPlanet() == Planet)
				{
					SpaceWorldManager->SetTravelState(EJTSSpaceTravelState::Landing);
				}
			}
			UE_LOG(LogJTSPlanetLanding, Log, TEXT("Landing Request accepted: Planet=%s Site=%s GroundDistance=%.1f Slope=%.1f"),
				*Planet->GetPlanetId().ToString(),
				*GetNameSafe(OutResult.LandingSite),
				OutResult.GroundDistance,
				OutResult.GroundSlopeDegrees);
			return true;
		}

		OutResult.bIsValid = false;
		OutResult.Failure = EJTSLandingValidationFailure::InvalidFlightState;
	}

	Spacecraft->CancelLandingRequest(OutResult.Failure);
	UE_LOG(LogJTSPlanetLanding, Warning, TEXT("Landing Request rejected: Planet=%s Spacecraft=%s Reason=%s"),
		*GetNameSafe(Planet),
		*GetNameSafe(Spacecraft),
		GetLandingValidationFailureName(OutResult.Failure));
	return false;
}

bool AJTSPlanetLandingManager::FindPlayerRespawnTransform(
	const AJTSSpacecraftActor* Spacecraft,
	FJTSPlayerRespawnTransformResult& OutResult) const
{
	OutResult = FJTSPlayerRespawnTransformResult();
	if (!IsValid(Spacecraft) || !Spacecraft->IsLanded())
	{
		return false;
	}

	AJTSPlanetAnchor* const Planet = Spacecraft->GetLandedPlanet();
	if (!IsValid(Planet))
	{
		return false;
	}

	const FVector SpacecraftLocation = Spacecraft->GetActorLocation();
	const float SearchRadius = Spacecraft->GetPlayerRespawnSearchRadius();
	const TArray<AJTSPlanetLandingSite*> LandingSites = GetLandingSitesForPlanet(Planet);
	TArray<FTransform> RandomCandidates;
	for (int32 AttemptIndex = 0; AttemptIndex < FMath::Max(1, RespawnRandomAttempts); ++AttemptIndex)
	{
		if (LandingSites.IsEmpty())
		{
			break;
		}

		AJTSPlanetLandingSite* const LandingSite = LandingSites[FMath::RandRange(0, LandingSites.Num() - 1)];
		if (!IsValid(LandingSite) || !LandingSite->IsLandingEnabled())
		{
			continue;
		}

		FVector AreaPoint;
		if (!LandingSite->FindRandomValidRespawnPoint(SpacecraftLocation, SearchRadius, AreaPoint)
			|| !IsLocationInsideLandingArea(Planet, AreaPoint))
		{
			continue;
		}

		FTransform CandidateTransform;
		if (BuildRespawnTransformAtAreaPoint(Spacecraft, Planet, AreaPoint, CandidateTransform, true))
		{
			RandomCandidates.Add(CandidateTransform);
			if (RandomCandidates.Num() >= 8)
			{
				break;
			}
		}
	}

	if (!RandomCandidates.IsEmpty())
	{
		OutResult.bIsValid = true;
		OutResult.Source = EJTSRespawnTransformSource::LandingAreaRandom;
		OutResult.Transform = RandomCandidates[FMath::RandRange(0, RandomCandidates.Num() - 1)];
		return true;
	}

	bool bFoundNearestPoint = false;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	FTransform NearestTransform;
	for (AJTSPlanetLandingSite* const LandingSite : LandingSites)
	{
		if (!IsValid(LandingSite) || !LandingSite->IsLandingEnabled())
		{
			continue;
		}

		FVector AreaPoint;
		if (!LandingSite->FindClosestPointInLandingArea(SpacecraftLocation, AreaPoint))
		{
			continue;
		}

		FTransform CandidateTransform;
		if (!BuildRespawnTransformAtAreaPoint(Spacecraft, Planet, AreaPoint, CandidateTransform, false))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(SpacecraftLocation, CandidateTransform.GetLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestTransform = CandidateTransform;
			bFoundNearestPoint = true;
		}
	}

	if (bFoundNearestPoint)
	{
		OutResult.bIsValid = true;
		OutResult.Source = EJTSRespawnTransformSource::LandingAreaNearest;
		OutResult.Transform = NearestTransform;
		return true;
	}

	if (Spacecraft->GetTopRespawnTransform(OutResult.Transform))
	{
		OutResult.bIsValid = true;
		OutResult.Source = EJTSRespawnTransformSource::SpacecraftTop;
		return true;
	}

	if (Spacecraft->GetExitRespawnTransform(OutResult.Transform))
	{
		OutResult.bIsValid = true;
		OutResult.Source = EJTSRespawnTransformSource::SpacecraftExit;
		return true;
	}

	return false;
}

bool AJTSPlanetLandingManager::RespawnPlayerAtLandedSpacecraft(APlayerController* PlayerController)
{
	AJTSSpacecraftActor* const Spacecraft = GetPlayerSpacecraft(PlayerController);
	if (!IsValid(PlayerController) || !IsValid(Spacecraft) || !Spacecraft->IsLanded())
	{
		UE_LOG(LogJTSPlanetLanding, Log, TEXT("Player Respawn waiting: PlayerController=%s Spacecraft=%s Reason=NoLandedSpacecraft"),
			*GetNameSafe(PlayerController), *GetNameSafe(Spacecraft));
		return false;
	}

	FJTSPlayerRespawnTransformResult RespawnResult;
	if (!Spacecraft->GetPlayerRespawnTransform(RespawnResult) || !RespawnResult.bIsValid)
	{
		UE_LOG(LogJTSPlanetLanding, Warning, TEXT("Player Respawn failed: PlayerController=%s Spacecraft=%s Reason=NoRespawnTransform"),
			*GetNameSafe(PlayerController), *GetNameSafe(Spacecraft));
		return false;
	}

	UWorld* const World = GetWorld();
	AGameModeBase* const GameMode = World != nullptr ? World->GetAuthGameMode<AGameModeBase>() : nullptr;
	if (!IsValid(GameMode))
	{
		return false;
	}

	if (APawn* const ExistingPawn = PlayerController->GetPawn())
	{
		PlayerController->UnPossess();
		ExistingPawn->Destroy();
	}

	GameMode->RestartPlayerAtTransform(PlayerController, RespawnResult.Transform);
	AJTSCharacter* const Character = Cast<AJTSCharacter>(PlayerController->GetPawn());
	AJTSPlanetAnchor* const Planet = Spacecraft->GetLandedPlanet();
	if (!IsValid(Character) || !IsValid(Planet))
	{
		return false;
	}

	Character->SetGameplayPlanet(Planet);
	Character->InitializePlanetFrame();
	Character->BeginPlanetFalling();
	RegisterPlayerSpacecraft(PlayerController, Spacecraft);
	UE_LOG(LogJTSPlanetLanding, Log, TEXT("Player Respawn Success: PlayerController=%s Spacecraft=%s Source=%d Location=%s"),
		*GetNameSafe(PlayerController),
		*GetNameSafe(Spacecraft),
		static_cast<int32>(RespawnResult.Source),
		*RespawnResult.Transform.GetLocation().ToCompactString());
	return true;
}

void AJTSPlanetLandingManager::RegisterPlayerSpacecraft(APlayerController* PlayerController, AJTSSpacecraftActor* Spacecraft)
{
	if (IsValid(PlayerController) && IsValid(Spacecraft))
	{
		PlayerSpacecraft.Add(PlayerController, Spacecraft);
	}
}

AJTSSpacecraftActor* AJTSPlanetLandingManager::GetPlayerSpacecraft(const APlayerController* PlayerController) const
{
	if (!IsValid(PlayerController))
	{
		return nullptr;
	}

	if (const TWeakObjectPtr<AJTSSpacecraftActor>* const Spacecraft = PlayerSpacecraft.Find(const_cast<APlayerController*>(PlayerController)))
	{
		return Spacecraft->Get();
	}

	return nullptr;
}

void AJTSPlanetLandingManager::SetDefaultSpacecraftClass(TSubclassOf<AJTSSpacecraftActor> InSpacecraftClass)
{
	if (InSpacecraftClass != nullptr)
	{
		DefaultSpacecraftClass = InSpacecraftClass;
	}
}

void AJTSPlanetLandingManager::DiscoverPersistentLandingSites()
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	for (TActorIterator<AJTSPlanetLandingSite> It(World); It; ++It)
	{
		RegisterLandingSite(*It);
	}
}

AJTSPlanetAnchor* AJTSPlanetLandingManager::ResolvePlanetForSpacecraft(AJTSSpacecraftActor* Spacecraft) const
{
	if (!IsValid(Spacecraft))
	{
		return nullptr;
	}

	if (AJTSPlanetAnchor* const FlightPlanet = Spacecraft->GetFlightPlanet())
	{
		return FlightPlanet;
	}

	if (AJTSSpaceWorldManager* const SpaceWorldManager = AJTSSpaceWorldManager::FindSpaceWorldManager(this))
	{
		if (AJTSPlanetAnchor* const NearestPlanet = SpaceWorldManager->FindNearestGameplayPlanet(Spacecraft->GetActorLocation(), false))
		{
			return NearestPlanet;
		}

		return SpaceWorldManager->GetCurrentPlanet();
	}

	return nullptr;
}

bool AJTSPlanetLandingManager::QueryLandingAvailability(
	AJTSSpacecraftActor* Spacecraft,
	FJTSPlanetLandingValidationResult& OutResult,
	AJTSPlanetAnchor*& OutPlanet) const
{
	OutResult = FJTSPlanetLandingValidationResult();
	OutPlanet = ResolvePlanetForSpacecraft(Spacecraft);
	if (!IsValid(Spacecraft) || !IsValid(OutPlanet))
	{
		OutResult.Failure = EJTSLandingValidationFailure::NoPlanet;
		return false;
	}

	const TArray<AJTSPlanetLandingSite*> LandingSites = GetLandingSitesForPlanet(OutPlanet);
	if (LandingSites.IsEmpty())
	{
		OutResult.Failure = EJTSLandingValidationFailure::NoLandingSite;
		return false;
	}

	const FVector RadialUp = OutPlanet->GetRadialUpVector(Spacecraft->GetActorLocation());
	float RequiredProbeDistance = FMath::Max(100.0f, InitialGroundProbeDistance);
	for (AJTSPlanetLandingSite* const LandingSite : LandingSites)
	{
		if (!IsValid(LandingSite) || !LandingSite->IsLandingEnabled())
		{
			continue;
		}

		const FJTSPlanetLandingValidationData ValidationData = LandingSite->GetLandingValidationData();
		RequiredProbeDistance = FMath::Max(
			RequiredProbeDistance,
			FMath::Max(ValidationData.MaxLandingHeight, ValidationData.TargetSurfaceProbeDistance)
				+ Spacecraft->GetLandingCollisionClearance(RadialUp) + 100.0f);
	}

	if (!Spacecraft->RefreshGroundInfo(OutPlanet, RequiredProbeDistance))
	{
		OutResult.Failure = EJTSLandingValidationFailure::NoSurface;
		return false;
	}

	const FJTSSpacecraftGroundInfo GroundInfo = Spacecraft->GetGroundInfo();
	if (!GroundInfo.bHasGround)
	{
		OutResult.Failure = EJTSLandingValidationFailure::NoSurface;
		return false;
	}

	bool bInsideAnyLandingArea = false;
	FJTSPlanetLandingValidationResult FirstFailure;
	for (AJTSPlanetLandingSite* const LandingSite : LandingSites)
	{
		if (!IsValid(LandingSite) || !LandingSite->IsLandingEnabled()
			|| !LandingSite->IsLocationInsideLandingArea(GroundInfo.GroundLocation))
		{
			continue;
		}

		bInsideAnyLandingArea = true;
		FJTSPlanetLandingValidationResult SiteResult;
		if (ValidateLandingSite(Spacecraft, OutPlanet, LandingSite, GroundInfo, SiteResult))
		{
			OutResult = SiteResult;
			return true;
		}

		if (FirstFailure.Failure == EJTSLandingValidationFailure::NoLandingSite)
		{
			FirstFailure = SiteResult;
		}
	}

	OutResult = bInsideAnyLandingArea ? FirstFailure : FJTSPlanetLandingValidationResult();
	if (!bInsideAnyLandingArea)
	{
		OutResult.Failure = EJTSLandingValidationFailure::OutsideLandingArea;
	}
	return false;
}

bool AJTSPlanetLandingManager::ValidateLandingSite(
	AJTSSpacecraftActor* Spacecraft,
	AJTSPlanetAnchor* Planet,
	AJTSPlanetLandingSite* LandingSite,
	const FJTSSpacecraftGroundInfo& GroundInfo,
	FJTSPlanetLandingValidationResult& OutResult) const
{
	OutResult = FJTSPlanetLandingValidationResult();
	OutResult.LandingSite = LandingSite;
	if (!IsValid(Spacecraft) || !IsValid(Planet) || !IsValid(LandingSite)
		|| !GroundInfo.bHasGround)
	{
		OutResult.Failure = EJTSLandingValidationFailure::InvalidLandingTarget;
		return false;
	}

	const FJTSPlanetLandingValidationData ValidationData = LandingSite->GetLandingValidationData();
	if (Spacecraft->GetCurrentSpeed() > ValidationData.MaxLandingSpeed)
	{
		OutResult.Failure = EJTSLandingValidationFailure::TooFast;
		return false;
	}

	OutResult.GroundDistance = GroundInfo.Distance;
	OutResult.GroundLocation = GroundInfo.GroundLocation;
	OutResult.GroundNormal = GroundInfo.SurfaceNormal.GetSafeNormal();
	OutResult.GroundSlopeDegrees = GroundInfo.SlopeDegrees;
	if (OutResult.GroundDistance > ValidationData.MaxLandingHeight)
	{
		OutResult.Failure = EJTSLandingValidationFailure::TooHigh;
		return false;
	}

	const FVector LandingUp = OutResult.GroundNormal;
	if (LandingUp.IsNearlyZero())
	{
		OutResult.Failure = EJTSLandingValidationFailure::InvalidLandingTarget;
		return false;
	}

	if (OutResult.GroundSlopeDegrees > ValidationData.MaxSlopeDegrees)
	{
		OutResult.Failure = EJTSLandingValidationFailure::TooSteep;
		return false;
	}

	// Surface Alignment intentionally replaces the historical attitude gate. A valid landing request
	// keeps its tangential heading but rotates the craft's local Z onto this real mesh normal.
	const FVector LandingForward = MakeTangentForward(Spacecraft->GetActorForwardVector(), LandingUp);
	const FQuat LandingRotation = FRotationMatrix::MakeFromXZ(LandingForward, LandingUp).ToQuat();
	OutResult.LandingClearance = Spacecraft->GetLandingCollisionClearanceForRotation(LandingRotation, LandingUp)
		+ FMath::Max(0.0f, ValidationData.SurfaceOffset);
	OutResult.LandingTransform = FTransform(
		LandingRotation,
		GroundInfo.GroundLocation + LandingUp * OutResult.LandingClearance);
	if (!Spacecraft->CanOccupyLandingTransform(OutResult.LandingTransform))
	{
		OutResult.Failure = EJTSLandingValidationFailure::CollisionBlocked;
		return false;
	}

	OutResult.bIsValid = true;
	OutResult.Failure = EJTSLandingValidationFailure::None;
	return true;
}

bool AJTSPlanetLandingManager::BuildRespawnTransformAtAreaPoint(
	const AJTSSpacecraftActor* Spacecraft,
	AJTSPlanetAnchor* Planet,
	const FVector& AreaPoint,
	FTransform& OutTransform,
	bool bRequireClearance) const
{
	if (!IsValid(Spacecraft) || !IsValid(Planet))
	{
		return false;
	}

	const FVector ProbeUp = Planet->GetRadialUpVector(AreaPoint);
	const float ProbeLift = FMath::Max(150.0f, Spacecraft->GetPlayerRespawnCapsuleHalfHeight() + Spacecraft->GetPlayerRespawnClearance());
	FJTSPlanetSurfaceHit SurfaceHit;
	if (!Planet->ProbeSurfaceAlongGravity(
		AreaPoint + ProbeUp * ProbeLift,
		FMath::Max(ProbeLift + 1.0f, RespawnSurfaceProbeDistance),
		SurfaceHit))
	{
		return false;
	}

	const FVector SurfaceUp = SurfaceHit.ImpactNormal.GetSafeNormal();
	const FVector Forward = MakeTangentForward(Spacecraft->GetActorForwardVector(), SurfaceUp);
	OutTransform = FTransform(
		FRotationMatrix::MakeFromXZ(Forward, SurfaceUp).ToQuat(),
		SurfaceHit.ImpactPoint + SurfaceUp * (Spacecraft->GetPlayerRespawnCapsuleHalfHeight() + Spacecraft->GetPlayerRespawnClearance()));
	return !bRequireClearance || IsRespawnTransformClear(Spacecraft, OutTransform);
}

bool AJTSPlanetLandingManager::IsRespawnTransformClear(const AJTSSpacecraftActor* Spacecraft, const FTransform& Transform) const
{
	UWorld* const World = GetWorld();
	if (World == nullptr || !IsValid(Spacecraft))
	{
		return false;
	}

	FCollisionQueryParams PlacementParams(SCENE_QUERY_STAT(JTSLandingRespawnPlacement), false, Spacecraft);
	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(
		Spacecraft->GetPlayerRespawnCapsuleRadius(),
		Spacecraft->GetPlayerRespawnCapsuleHalfHeight());
	return !World->OverlapBlockingTestByChannel(
		Transform.GetLocation(),
		Transform.GetRotation(),
		ECC_Pawn,
		CapsuleShape,
		PlacementParams);
}

bool AJTSPlanetLandingManager::SpawnAndConfigureCharacter(
	APlayerController* PlayerController,
	AJTSPlanetAnchor* Planet,
	const FTransform& SpawnTransform,
	AJTSCharacter*& OutCharacter) const
{
	OutCharacter = nullptr;
	if (!IsValid(PlayerController) || !IsValid(Planet))
	{
		return false;
	}

	AJTSCharacter* Character = Cast<AJTSCharacter>(PlayerController->GetPawn());
	if (!IsValid(Character))
	{
		UWorld* const World = GetWorld();
		AGameModeBase* const GameMode = World != nullptr ? World->GetAuthGameMode<AGameModeBase>() : nullptr;
		if (!IsValid(GameMode))
		{
			return false;
		}

		if (APawn* const ExistingPawn = PlayerController->GetPawn())
		{
			PlayerController->UnPossess();
			ExistingPawn->Destroy();
		}

		GameMode->RestartPlayerAtTransform(PlayerController, SpawnTransform);
		Character = Cast<AJTSCharacter>(PlayerController->GetPawn());
	}
	else
	{
		Character->SetActorLocationAndRotation(
			SpawnTransform.GetLocation(),
			SpawnTransform.GetRotation(),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}

	if (!IsValid(Character))
	{
		return false;
	}

	Character->SetGameplayPlanet(Planet);
	Character->InitializePlanetFrame();
	Character->BeginPlanetFalling();
	OutCharacter = Character;
	return true;
}

AJTSSpacecraftActor* AJTSPlanetLandingManager::FindOrSpawnArrivalSpacecraft(
	AJTSPlanetAnchor* Planet,
	const FTransform& SpawnTransform,
	const FVector& PreferredSurfaceLocation)
{
	if (!IsValid(Planet))
	{
		return nullptr;
	}

	if (const TWeakObjectPtr<AJTSSpacecraftActor>* const ExistingSpacecraft = ArrivalSpacecraftByPlanet.Find(Planet))
	{
		if (AJTSSpacecraftActor* const Spacecraft = ExistingSpacecraft->Get(); IsValid(Spacecraft))
		{
			Spacecraft->InitializeForPlanetArrival(Planet);
			ParkArrivalSpacecraftOnSurface(Spacecraft, Planet, PreferredSurfaceLocation);
			return Spacecraft;
		}
	}

	UWorld* const World = GetWorld();
	if (World == nullptr || World->PersistentLevel == nullptr)
	{
		return nullptr;
	}

	TSubclassOf<AJTSSpacecraftActor> SpacecraftClass;
	if (UJTSGameInstance* const GameInstance = World->GetGameInstance<UJTSGameInstance>();
		IsValid(GameInstance) && GameInstance->HasPersistedSpacecraftClass())
	{
		SpacecraftClass = GameInstance->GetPersistedSpacecraftClass();
	}
	if (SpacecraftClass == nullptr)
	{
		SpacecraftClass = DefaultSpacecraftClass;
	}
	if (SpacecraftClass == nullptr)
	{
		SpacecraftClass = AJTSSpacecraftActor::StaticClass();
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.OverrideLevel = World->PersistentLevel;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AJTSSpacecraftActor* const Spacecraft = World->SpawnActor<AJTSSpacecraftActor>(
		SpacecraftClass,
		SpawnTransform,
		SpawnParameters);
	if (!IsValid(Spacecraft))
	{
		return nullptr;
	}

	Spacecraft->RestorePersistentStorage();
	Spacecraft->InitializeForPlanetArrival(Planet);
	ParkArrivalSpacecraftOnSurface(Spacecraft, Planet, PreferredSurfaceLocation);
	ArrivalSpacecraftByPlanet.Add(Planet, Spacecraft);
	return Spacecraft;
}

bool AJTSPlanetLandingManager::ParkArrivalSpacecraftOnSurface(
	AJTSSpacecraftActor* Spacecraft,
	AJTSPlanetAnchor* Planet,
	const FVector& PreferredSurfaceLocation) const
{
	if (!IsValid(Spacecraft) || !IsValid(Planet))
	{
		return false;
	}

	const auto ResolveAndPark = [this, Spacecraft, Planet](const FVector& QueryLocation, const TCHAR* Source)
	{
		const float ProbeDistance = FMath::Max(
			FMath::Max(100.0f, InitialGroundProbeDistance),
			FMath::Abs(Planet->GetApproximateAltitude(QueryLocation))
				+ FMath::Max(1000.0f, InitialGroundProbeDistance * 0.25f));
		FJTSPlanetSurfaceFrame SurfaceFrame;
		if (!Planet->GetSurfaceFrameAlongGravity(
			QueryLocation,
			ProbeDistance,
			Spacecraft->GetActorForwardVector(),
			SurfaceFrame))
		{
			return false;
		}

		if (!Spacecraft->SnapSpacecraftToSurfaceTransform(Planet, SurfaceFrame.Transform))
		{
			return false;
		}

		UE_LOG(LogJTSPlanetLanding, Log, TEXT("Initial Spacecraft surface parking complete: Source=%s Spacecraft=%s Planet=%s Location=%s"),
			Source,
			*GetNameSafe(Spacecraft),
			*Planet->GetPlanetId().ToString(),
			*Spacecraft->GetActorLocation().ToCompactString());
		return true;
	};

	if (ResolveAndPark(Spacecraft->GetActorLocation(), TEXT("SpacecraftArrival")))
	{
		return true;
	}

	if (ResolveAndPark(PreferredSurfaceLocation, TEXT("PlayerSurfaceFallback")))
	{
		return true;
	}

	if (ParkArrivalSpacecraftOnPhysicalGround(Spacecraft, Planet, PreferredSurfaceLocation))
	{
		return true;
	}

	UE_LOG(LogJTSPlanetLanding, Warning, TEXT("Initial Spacecraft surface parking failed: Spacecraft=%s Planet=%s Reason=NoSurfaceAtArrivalOrPlayer"),
		*GetNameSafe(Spacecraft), *Planet->GetPlanetId().ToString());
	return false;
}

bool AJTSPlanetLandingManager::ParkArrivalSpacecraftOnPhysicalGround(
	AJTSSpacecraftActor* Spacecraft,
	AJTSPlanetAnchor* Planet,
	const FVector& QueryLocation) const
{
	UWorld* const World = GetWorld();
	if (!IsValid(World) || !IsValid(Spacecraft) || !IsValid(Planet))
	{
		return false;
	}

	const FVector SurfaceUp = Planet->GetRadialUpVector(QueryLocation).GetSafeNormal();
	const FVector GravityDirection = Planet->GetGravityDirection(QueryLocation).GetSafeNormal();
	if (SurfaceUp.IsNearlyZero() || GravityDirection.IsNearlyZero())
	{
		return false;
	}

	const float TraceDistance = FMath::Max(
		FMath::Max(100.0f, InitialGroundProbeDistance),
		FMath::Abs(Planet->GetApproximateAltitude(QueryLocation))
			+ FMath::Max(1000.0f, InitialGroundProbeDistance * 0.25f));
	const FVector TraceStart = QueryLocation + SurfaceUp * 25.0f;
	const FVector TraceEnd = TraceStart + GravityDirection * TraceDistance;

	FCollisionObjectQueryParams GroundObjectQuery;
	GroundObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	GroundObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams GroundTraceParameters(SCENE_QUERY_STAT(JTSArrivalPhysicalGroundTrace), true, this);
	GroundTraceParameters.AddIgnoredActor(Spacecraft);
	GroundTraceParameters.bTraceComplex = true;

	const auto FindGroundHit = [&World, &GroundObjectQuery, &GroundTraceParameters, TraceStart, TraceEnd, SurfaceUp](bool bTraceComplex, FHitResult& OutGroundHit)
	{
		GroundTraceParameters.bTraceComplex = bTraceComplex;
		TArray<FHitResult> GroundHits;
		World->LineTraceMultiByObjectType(
			GroundHits,
			TraceStart,
			TraceEnd,
			GroundObjectQuery,
			GroundTraceParameters);

		for (const FHitResult& CandidateHit : GroundHits)
		{
			if (!CandidateHit.bBlockingHit || !IsValid(CandidateHit.GetComponent()))
			{
				continue;
			}

			FVector CandidateNormal = CandidateHit.ImpactNormal.GetSafeNormal();
			if (CandidateNormal.IsNearlyZero())
			{
				CandidateNormal = CandidateHit.Normal.GetSafeNormal();
			}

			// An arrival trace always travels toward the planet. Accept an outward-facing real ground
			// surface only, never the underside of a mesh or an unrelated overhead blocker.
			if (CandidateNormal.IsNearlyZero()
				|| FVector::DotProduct(CandidateNormal, SurfaceUp) < 0.15f)
			{
				continue;
			}

			OutGroundHit = CandidateHit;
			return true;
		}

		return false;
	};

	FHitResult GroundHit;
	if (!FindGroundHit(true, GroundHit) && !FindGroundHit(false, GroundHit))
	{
		return false;
	}

	FVector GroundUp = GroundHit.ImpactNormal.GetSafeNormal();
	if (GroundUp.IsNearlyZero())
	{
		GroundUp = GroundHit.Normal.GetSafeNormal();
	}
	if (GroundUp.IsNearlyZero())
	{
		return false;
	}

	const FVector GroundForward = MakeTangentForward(Spacecraft->GetActorForwardVector(), GroundUp);
	const FTransform GroundSurfaceTransform(
		FRotationMatrix::MakeFromXZ(GroundForward, GroundUp).ToQuat(),
		GroundHit.ImpactPoint);
	if (!Spacecraft->SnapSpacecraftToSurfaceTransform(Planet, GroundSurfaceTransform))
	{
		return false;
	}

	UE_LOG(LogJTSPlanetLanding, Warning, TEXT("Initial Spacecraft surface parking used physical-ground fallback: Spacecraft=%s Planet=%s GroundActor=%s GroundComponent=%s Location=%s Normal=%s ConfiguredSurfaceActor=%s ConfiguredSurfaceComponent=%s. Update PlanetAnchor GameplaySurfaceActor/Component to this ground for future generic surface queries."),
		*GetNameSafe(Spacecraft),
		*Planet->GetPlanetId().ToString(),
		*GetNameSafe(GroundHit.GetActor()),
		*GetNameSafe(GroundHit.GetComponent()),
		*GroundHit.ImpactPoint.ToCompactString(),
		*GroundUp.ToCompactString(),
		*GetNameSafe(Planet->GetGameplaySurfaceActor()),
		*GetNameSafe(Planet->GetGameplaySurfaceComponent()));
	return true;
}

bool AJTSPlanetLandingManager::ResolveArrivalTransforms(
	APlayerController* PlayerController,
	AJTSPlanetAnchor* Planet,
	FTransform& OutPlayerTransform,
	FTransform& OutSpacecraftTransform) const
{
	if (!IsValid(PlayerController) || !IsValid(Planet))
	{
		return false;
	}

	AJTSPlanetArrivalAnchor* BestArrivalAnchor = nullptr;
	UWorld* const World = GetWorld();
	if (World != nullptr)
	{
		for (TActorIterator<AJTSPlanetArrivalAnchor> It(World); It; ++It)
		{
			AJTSPlanetArrivalAnchor* const Candidate = *It;
			if (!IsValid(Candidate) || !Candidate->IsArrivalEnabled() || Candidate->GetPlanetAnchor() != Planet)
			{
				continue;
			}

			if (!IsValid(BestArrivalAnchor)
				|| Candidate->GetPriority() > BestArrivalAnchor->GetPriority()
				|| (Candidate->GetPriority() == BestArrivalAnchor->GetPriority()
					&& Candidate->GetPathName() < BestArrivalAnchor->GetPathName()))
			{
				BestArrivalAnchor = Candidate;
			}
		}
	}

	if (IsValid(BestArrivalAnchor))
	{
		OutPlayerTransform = BestArrivalAnchor->GetPlayerArrivalTransform();
		OutSpacecraftTransform = BestArrivalAnchor->GetSpacecraftArrivalTransform();
		UE_LOG(LogJTSPlanetLanding, Log, TEXT("Landing Anchor: %s Planet=%s"),
			*GetNameSafe(BestArrivalAnchor), *Planet->GetPlanetId().ToString());
		return true;
	}

	AGameModeBase* const GameMode = World != nullptr ? World->GetAuthGameMode<AGameModeBase>() : nullptr;
	AActor* const PlayerStart = IsValid(GameMode) ? GameMode->FindPlayerStart(PlayerController) : nullptr;
	OutPlayerTransform = IsValid(PlayerStart) ? PlayerStart->GetActorTransform() : Planet->GetApproachEntryTransform();
	const FVector SurfaceUp = Planet->GetRadialUpVector(OutPlayerTransform.GetLocation());
	const FVector SurfaceForward = Planet->ProjectDirectionToSurfaceTangent(
		OutPlayerTransform.GetUnitAxis(EAxis::X),
		OutPlayerTransform.GetLocation());
	const FQuat SurfaceRotation = FRotationMatrix::MakeFromXZ(SurfaceForward, SurfaceUp).ToQuat();
	OutSpacecraftTransform = FTransform(
		SurfaceRotation,
		OutPlayerTransform.GetLocation() + SurfaceRotation.RotateVector(GenericArrivalSpacecraftOffset));
	UE_LOG(LogJTSPlanetLanding, Warning, TEXT("Landing Anchor: <none>; using PlayerStart-adjacent surface fallback for Planet=%s."),
		*Planet->GetPlanetId().ToString());
	return true;
}

void AJTSPlanetLandingManager::LogGroundDetectionResult(
	AJTSPlanetAnchor* Planet,
	const FVector& Location,
	const TCHAR* Context) const
{
	if (!IsValid(Planet))
	{
		UE_LOG(LogJTSPlanetLanding, Log, TEXT("Ground Detection Result: Context=%s Planet=<none> Hit=FALSE"), Context);
		return;
	}

	FJTSPlanetSurfaceHit SurfaceHit;
	const bool bHit = Planet->ProbeSurfaceAlongGravity(Location, InitialGroundProbeDistance, SurfaceHit);
	UE_LOG(LogJTSPlanetLanding, Log, TEXT("Ground Detection Result: Context=%s Planet=%s Hit=%s Distance=%.1f Location=%s Normal=%s"),
		Context,
		*Planet->GetPlanetId().ToString(),
		bHit ? TEXT("TRUE") : TEXT("FALSE"),
		bHit ? SurfaceHit.Distance : -1.0f,
		bHit ? *SurfaceHit.ImpactPoint.ToCompactString() : TEXT("<none>"),
		bHit ? *SurfaceHit.ImpactNormal.ToCompactString() : TEXT("<none>"));
}
