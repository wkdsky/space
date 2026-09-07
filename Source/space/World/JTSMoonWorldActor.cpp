#include "JTSMoonWorldActor.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "GameFramework/Pawn.h"
#include "space/Modes/JTSMoonGameMode.h"

namespace
{
	const FName BendOriginXName(TEXT("BendOriginX"));
	const FName BendOriginYName(TEXT("BendOriginY"));
	const FName WorldBendEnabledName(TEXT("WorldBendEnabled"));
	const FName FlatRadiusName(TEXT("FlatRadius"));
	const FName TransitionWidthName(TEXT("TransitionWidth"));
	const FName CurveRadiusName(TEXT("CurveRadius"));
	const FName BendMaxDistanceName(TEXT("BendMaxDistance"));
}

AJTSMoonWorldActor::AJTSMoonWorldActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	SetActorTickEnabled(true);
}

float AJTSMoonWorldActor::GetMapSizeX() const
{
	return FMath::IsFinite(MapSizeX) ? FMath::Max(1.0f, MapSizeX) : 24000.0f;
}

float AJTSMoonWorldActor::GetMapSizeY() const
{
	return FMath::IsFinite(MapSizeY) ? FMath::Max(1.0f, MapSizeY) : 24000.0f;
}

FVector2D AJTSMoonWorldActor::GetMapSize2D() const
{
	return FVector2D(GetMapSizeX(), GetMapSizeY());
}

bool AJTSMoonWorldActor::IsWorldBendEnabled() const
{
	return bEnableWorldBend;
}

float AJTSMoonWorldActor::GetFlatRadius() const
{
	return FMath::IsFinite(FlatRadius) ? FMath::Max(0.0f, FlatRadius) : 0.0f;
}

float AJTSMoonWorldActor::GetBendTransitionWidth() const
{
	return FMath::IsFinite(BendTransitionWidth) ? FMath::Max(0.0f, BendTransitionWidth) : 0.0f;
}

float AJTSMoonWorldActor::GetVisualCurveRadius() const
{
	return FMath::IsFinite(VisualCurveRadius) ? FMath::Max(1.0f, VisualCurveRadius) : 1.0f;
}

float AJTSMoonWorldActor::GetBendMaxDistance() const
{
	return FMath::IsFinite(BendMaxDistance) ? FMath::Max(1.0f, BendMaxDistance) : 1.0f;
}

float AJTSMoonWorldActor::GetMaximumVisualDisplacement() const
{
	const float CurvedDistance = FMath::Max(0.0f, GetBendMaxDistance() - GetFlatRadius());
	return (CurvedDistance * CurvedDistance) / (2.0f * GetVisualCurveRadius());
}

float AJTSMoonWorldActor::GetRecommendedBoundsScale(float BaseBoundsRadius) const
{
	const float SafeBaseRadius = FMath::Max(1.0f, FMath::Abs(BaseBoundsRadius));
	const float DisplacementRatio = GetMaximumVisualDisplacement() / SafeBaseRadius;
	return FMath::Clamp(1.0f + DisplacementRatio + 0.05f, 1.05f, 64.0f);
}

FVector AJTSMoonWorldActor::GetMoonVisualWorldPosition(
	const FVector& PhysicalWorldPosition,
	const FVector& ViewerLocation) const
{
	// Keep this in lockstep with Shaders/JTSFakeMoonBend.ush::JTSFakeMoon_GetWPO.
	if (!IsWorldBendEnabled())
	{
		return PhysicalWorldPosition;
	}

	const float CurveRadius = GetVisualCurveRadius();
	const float MaxDistance = GetBendMaxDistance();
	if (CurveRadius <= 0.0001f || MaxDistance <= 0.0f)
	{
		return PhysicalWorldPosition;
	}

	const FVector2D DeltaXY(
		PhysicalWorldPosition.X - ViewerLocation.X,
		PhysicalWorldPosition.Y - ViewerLocation.Y);
	const float Distance = FMath::Sqrt(FMath::Max(DeltaXY.SizeSquared(), 0.0f));
	const float SafeFlatRadius = FMath::Max(GetFlatRadius(), 0.0f);
	const float DistanceAfterFlat = FMath::Max(Distance - SafeFlatRadius, 0.0f);

	const float TransitionWidth = GetBendTransitionWidth();
	float TransitionFactor = 1.0f;
	if (TransitionWidth > 0.0f)
	{
		const float TransitionT = FMath::Clamp(DistanceAfterFlat / TransitionWidth, 0.0f, 1.0f);
		TransitionFactor = TransitionT * TransitionT * (3.0f - (2.0f * TransitionT));
	}
	else if (DistanceAfterFlat <= 0.0f)
	{
		TransitionFactor = 0.0f;
	}

	float EffectiveDistance = DistanceAfterFlat * TransitionFactor;
	EffectiveDistance = SmoothClampBendDistance(
		EffectiveDistance,
		FMath::Max(MaxDistance - SafeFlatRadius, 0.0f),
		TransitionWidth);

	const float VerticalOffset = -(EffectiveDistance * EffectiveDistance) / (2.0f * FMath::Max(CurveRadius, 0.0001f));
	return PhysicalWorldPosition + FVector(0.0f, 0.0f, VerticalOffset);
}

void AJTSMoonWorldActor::BeginPlay()
{
	Super::BeginPlay();

	bMissingCollectionLogged = false;
	if (!IsMoonWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("JTSMoonWorldActor is present outside AJTSMoonGameMode; its bend driver is disabled."));
		SetActorTickEnabled(false);
		return;
	}

	UpdateBendMaterialParameters();
}

void AJTSMoonWorldActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateBendMaterialParameters();
}

bool AJTSMoonWorldActor::IsMoonWorld() const
{
	const UWorld* const World = GetWorld();
	return World != nullptr && World->GetAuthGameMode<AJTSMoonGameMode>() != nullptr;
}

float AJTSMoonWorldActor::SmoothClampBendDistance(
	float DistanceAfterFlat,
	float MaxDistanceAfterFlat,
	float TransitionWidth)
{
	const float SafeMaxDistance = FMath::Max(MaxDistanceAfterFlat, 0.0f);
	if (SafeMaxDistance <= 0.0f)
	{
		return 0.0f;
	}

	const float SafeDistance = FMath::Max(DistanceAfterFlat, 0.0f);
	const float ClampWidth = FMath::Min(
		FMath::Max(TransitionWidth, 1.0f),
		FMath::Max(SafeMaxDistance * 0.25f, 1.0f));
	const float ClampStart = FMath::Max(0.0f, SafeMaxDistance - ClampWidth);
	if (SafeDistance <= ClampStart)
	{
		return SafeDistance;
	}

	const float ClampT = FMath::Clamp(
		(FMath::Min(SafeDistance, SafeMaxDistance) - ClampStart) / FMath::Max(ClampWidth, 0.0001f),
		0.0f,
		1.0f);
	const float SmoothT = ClampT * ClampT * (3.0f - (2.0f * ClampT));
	return FMath::Lerp(FMath::Min(SafeDistance, SafeMaxDistance), SafeMaxDistance, SmoothT);
}

void AJTSMoonWorldActor::PublishScalar(
	UMaterialParameterCollectionInstance* Instance,
	FName ParameterName,
	float Value,
	float& CachedValue,
	bool& bHasCachedValue) const
{
	if (Instance == nullptr || (bHasCachedValue && FMath::IsNearlyEqual(CachedValue, Value, 0.0001f)))
	{
		return;
	}

	if (Instance->SetScalarParameterValue(ParameterName, Value))
	{
		CachedValue = Value;
		bHasCachedValue = true;
	}
}

void AJTSMoonWorldActor::UpdateBendMaterialParameters()
{
	if (!IsMoonWorld() || !bDriveMaterialParameterCollection)
	{
		return;
	}

	UWorld* const World = GetWorld();
	if (World == nullptr || BendParameterCollection == nullptr)
	{
		if (!bMissingCollectionLogged && BendParameterCollection == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("JTSMoonWorldActor has no MPC assigned. Assign MPC_JTSFakeMoon to publish Fake Moon bend parameters."));
			bMissingCollectionLogged = true;
		}
		return;
	}

	UMaterialParameterCollectionInstance* const Instance = World->GetParameterCollectionInstance(BendParameterCollection);
	if (Instance == nullptr)
	{
		return;
	}

	const APawn* const LocalPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	const FVector PlayerLocation = IsValid(LocalPawn) ? LocalPawn->GetActorLocation() : FVector::ZeroVector;
	PublishScalar(Instance, BendOriginXName, PlayerLocation.X, CachedBendOriginX, bHasCachedBendOriginX);
	PublishScalar(Instance, BendOriginYName, PlayerLocation.Y, CachedBendOriginY, bHasCachedBendOriginY);
	PublishScalar(Instance, WorldBendEnabledName, bEnableWorldBend ? 1.0f : 0.0f, CachedWorldBendEnabled, bHasCachedWorldBendEnabled);
	PublishScalar(Instance, FlatRadiusName, GetFlatRadius(), CachedFlatRadius, bHasCachedFlatRadius);
	PublishScalar(Instance, TransitionWidthName, GetBendTransitionWidth(), CachedTransitionWidth, bHasCachedTransitionWidth);
	PublishScalar(Instance, CurveRadiusName, GetVisualCurveRadius(), CachedCurveRadius, bHasCachedCurveRadius);
	PublishScalar(Instance, BendMaxDistanceName, GetBendMaxDistance(), CachedBendMaxDistance, bHasCachedBendMaxDistance);
	}
