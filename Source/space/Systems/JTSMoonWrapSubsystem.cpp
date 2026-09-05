#include "JTSMoonWrapSubsystem.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "space/World/JTSMoonWorldActor.h"

void UJTSMoonWrapSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RefreshConfiguration();
}

FVector2D UJTSMoonWrapSubsystem::GetMapSize2D() const
{
	if (const AJTSMoonWorldActor* const Configuration = FindConfigurationActor())
	{
		CachedMapSize = Configuration->GetMapSize2D();
	}

	CachedMapSize.X = FMath::IsFinite(CachedMapSize.X) ? FMath::Max(1.0f, CachedMapSize.X) : 24000.0f;
	CachedMapSize.Y = FMath::IsFinite(CachedMapSize.Y) ? FMath::Max(1.0f, CachedMapSize.Y) : 24000.0f;
	return CachedMapSize;
}

FVector2D UJTSMoonWrapSubsystem::CanonicalizePosition2D(const FVector2D& Position) const
{
	const FVector2D MapSize = GetMapSize2D();
	return FVector2D(
		CanonicalizeAxis(Position.X, MapSize.X),
		CanonicalizeAxis(Position.Y, MapSize.Y));
}

FVector2D UJTSMoonWrapSubsystem::ShortestWrappedDelta2D(const FVector2D& From, const FVector2D& To) const
{
	const FVector2D MapSize = GetMapSize2D();
	const FVector2D CanonicalFrom = CanonicalizePosition2D(From);
	const FVector2D CanonicalTo = CanonicalizePosition2D(To);
	return FVector2D(
		ShortestWrappedAxisDelta(CanonicalFrom.X, CanonicalTo.X, MapSize.X),
		ShortestWrappedAxisDelta(CanonicalFrom.Y, CanonicalTo.Y, MapSize.Y));
}

float UJTSMoonWrapSubsystem::WrappedDistance2D(const FVector2D& From, const FVector2D& To) const
{
	return ShortestWrappedDelta2D(From, To).Size();
}

FVector2D UJTSMoonWrapSubsystem::GetLogicalPositionFromWorld(const FVector& WorldPosition) const
{
	return CanonicalizePosition2D(FVector2D(WorldPosition.X, WorldPosition.Y));
}

FVector2D UJTSMoonWrapSubsystem::GetNearestPhysicalImage(
	const FVector2D& PlayerPhysicalXY,
	const FVector2D& ActorLogicalXY) const
{
	const FVector2D PlayerLogicalXY = CanonicalizePosition2D(PlayerPhysicalXY);
	const FVector2D Delta = ShortestWrappedDelta2D(PlayerLogicalXY, ActorLogicalXY);
	return PlayerPhysicalXY + Delta;
}

bool UJTSMoonWrapSubsystem::IsConfiguredForMoon() const
{
	return FindConfigurationActor() != nullptr;
}

void UJTSMoonWrapSubsystem::RefreshConfiguration() const
{
	ConfigurationActor.Reset();
	CachedMapSize = FVector2D(24000.0f, 24000.0f);
	if (const AJTSMoonWorldActor* const Configuration = FindConfigurationActor())
	{
		CachedMapSize = Configuration->GetMapSize2D();
	}
}

const AJTSMoonWorldActor* UJTSMoonWrapSubsystem::FindConfigurationActor() const
{
	if (ConfigurationActor.IsValid())
	{
		return ConfigurationActor.Get();
	}

	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	for (TActorIterator<AJTSMoonWorldActor> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			ConfigurationActor = *It;
			return *It;
		}
	}

	return nullptr;
}

float UJTSMoonWrapSubsystem::CanonicalizeAxis(float Value, float Size)
{
	if (!FMath::IsFinite(Value) || !FMath::IsFinite(Size) || Size <= 0.0f)
	{
		return 0.0f;
	}

	const float HalfSize = Size * 0.5f;
	float Offset = FMath::Fmod(Value + HalfSize, Size);
	if (Offset < 0.0f)
	{
		Offset += Size;
	}

	const float CanonicalValue = Offset - HalfSize;
	return CanonicalValue < HalfSize ? CanonicalValue : -HalfSize;
}

float UJTSMoonWrapSubsystem::ShortestWrappedAxisDelta(float From, float To, float Size)
{
	if (!FMath::IsFinite(From) || !FMath::IsFinite(To) || !FMath::IsFinite(Size) || Size <= 0.0f)
	{
		return 0.0f;
	}

	const float HalfSize = Size * 0.5f;
	float Delta = To - From;
	if (Delta > HalfSize)
	{
		Delta -= Size;
	}
	else if (Delta < -HalfSize)
	{
		Delta += Size;
	}
	return Delta;
}
