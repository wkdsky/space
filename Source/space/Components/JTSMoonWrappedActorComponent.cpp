#include "JTSMoonWrappedActorComponent.h"

#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "space/Modes/JTSMoonGameMode.h"
#include "space/Systems/JTSMoonWrapSubsystem.h"
#include "space/World/JTSMoonWorldActor.h"

UJTSMoonWrappedActorComponent::UJTSMoonWrappedActorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

FVector2D UJTSMoonWrappedActorComponent::GetLogicalPosition2D() const
{
	return LogicalPosition2D;
}

void UJTSMoonWrappedActorComponent::SetLogicalPosition2D(const FVector2D& NewLogicalPosition2D)
{
	LogicalPosition2D = NewLogicalPosition2D;
	bUseActorLocationAsInitialLogicalPosition = false;
	RefreshPhysicalImage();
}

void UJTSMoonWrappedActorComponent::SetLogicalPositionFromWorld()
{
	if (AActor* const Owner = GetOwner())
	{
		if (UJTSMoonWrapSubsystem* const Subsystem = WrapSubsystem.Get())
		{
			LogicalPosition2D = Subsystem->GetLogicalPositionFromWorld(Owner->GetActorLocation());
			bUseActorLocationAsInitialLogicalPosition = false;
		}
	}
}

bool UJTSMoonWrappedActorComponent::IsMoonWrappingEnabled() const
{
	return bMoonWrappingEnabled;
}

void UJTSMoonWrappedActorComponent::BeginPlay()
{
	Super::BeginPlay();
	ResolveMoonWorld();

	if (bMoonWrappingEnabled)
	{
		SetComponentTickEnabled(true);
		ApplyBendRenderingSettings();
		if (bUseActorLocationAsInitialLogicalPosition)
		{
			SetLogicalPositionFromWorld();
		}
		else if (UJTSMoonWrapSubsystem* const Subsystem = WrapSubsystem.Get())
		{
			LogicalPosition2D = Subsystem->CanonicalizePosition2D(LogicalPosition2D);
		}
		RefreshPhysicalImage();
	}
	else
	{
		SetComponentTickEnabled(false);
	}
}

void UJTSMoonWrappedActorComponent::TickComponent(
	float DeltaTime,
	enum ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bMoonWrappingEnabled || !FakeWorld.IsValid() || !WrapSubsystem.IsValid())
	{
		ResolveMoonWorld();
	}
	if (bMoonWrappingEnabled)
	{
		ApplyBendRenderingSettings();
		RefreshPhysicalImage();
	}
}

void UJTSMoonWrappedActorComponent::ResolveMoonWorld()
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		FakeWorld.Reset();
		WrapSubsystem.Reset();
		bMoonWrappingEnabled = false;
		return;
	}

	AJTSMoonWorldActor* FoundFakeWorld = FakeWorld.Get();
	if (!IsValid(FoundFakeWorld))
	{
		for (TActorIterator<AJTSMoonWorldActor> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				FoundFakeWorld = *It;
				break;
			}
		}
	}

	FakeWorld = FoundFakeWorld;
	WrapSubsystem = World->GetSubsystem<UJTSMoonWrapSubsystem>();
	bMoonWrappingEnabled = IsValid(FoundFakeWorld)
		&& World->GetAuthGameMode<AJTSMoonGameMode>() != nullptr
		&& WrapSubsystem.IsValid();
	if (!bMoonWrappingEnabled)
	{
		FakeWorld.Reset();
		WrapSubsystem.Reset();
		bRenderingSettingsApplied = false;
	}
}

bool UJTSMoonWrappedActorComponent::IsOwnerLocalControlled() const
{
	const AActor* const Owner = GetOwner();
	if (Owner == nullptr)
	{
		return true;
	}

	if (const APawn* const PawnOwner = Cast<APawn>(Owner))
	{
		return PawnOwner->IsLocallyControlled();
	}

	const APawn* const LocalPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	return IsValid(LocalPawn) && LocalPawn == Owner;
}

void UJTSMoonWrappedActorComponent::RefreshPhysicalImage()
{
	if (!bMoonWrappingEnabled || IsOwnerLocalControlled())
	{
		return;
	}

	AActor* const Owner = GetOwner();
	const UJTSMoonWrapSubsystem* const Subsystem = WrapSubsystem.Get();
	const APawn* const LocalPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!IsValid(Owner) || !IsValid(Subsystem) || !IsValid(LocalPawn))
	{
		return;
	}

	const FVector PlayerLocation = LocalPawn->GetActorLocation();
	const FVector2D PlayerPhysicalXY(PlayerLocation.X, PlayerLocation.Y);
	const FVector2D NewPhysicalXY = Subsystem->GetNearestPhysicalImage(PlayerPhysicalXY, LogicalPosition2D);
	const FVector CurrentLocation = Owner->GetActorLocation();
	const FVector2D CurrentPhysicalXY(CurrentLocation.X, CurrentLocation.Y);
	const bool bImageChanged = !bHasPhysicalImage || !CurrentPhysicalXY.Equals(NewPhysicalXY, 0.01f);

	if (bImageChanged)
	{
		const float PreviousDistance = bHasPhysicalImage ? FVector2D::Distance(PlayerPhysicalXY, CurrentPhysicalXY) : 0.0f;
		const float NewDistance = FVector2D::Distance(PlayerPhysicalXY, NewPhysicalXY);
		Owner->SetActorLocation(
			FVector(NewPhysicalXY.X, NewPhysicalXY.Y, CurrentLocation.Z),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		bHasPhysicalImage = true;

		if (bEnableDebugLogging && PreviousDistance > 0.0f)
		{
			const FVector2D MapSize = Subsystem->GetMapSize2D();
			const float HalfMapSize = FMath::Min(MapSize.X, MapSize.Y) * 0.5f;
			UE_LOG(LogTemp, Log, TEXT("JTSMoonWrappedActorComponent switched periodic image: old distance %.1f cm, new distance %.1f cm, half-map %.1f cm."), PreviousDistance, NewDistance, HalfMapSize);
		}
	}
}

void UJTSMoonWrappedActorComponent::SetFakeMoonBendMaterial(UMaterialInterface* NewMaterial)
{
	FakeMoonBendMaterial = NewMaterial;
	bRenderingSettingsApplied = false;
	ApplyBendRenderingSettings();
}

void UJTSMoonWrappedActorComponent::ApplyBendRenderingSettings()
{
	if (!bMoonWrappingEnabled || bRenderingSettingsApplied)
	{
		return;
	}

	AActor* const Owner = GetOwner();
	const AJTSMoonWorldActor* const WorldConfig = FakeWorld.Get();
	if (!IsValid(Owner) || !IsValid(WorldConfig))
	{
		return;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Owner->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* const PrimitiveComponent : PrimitiveComponents)
	{
		if (!IsValid(PrimitiveComponent))
		{
			continue;
		}

		if (FakeMoonBendMaterial != nullptr && PrimitiveComponent->GetNumMaterials() > 0)
		{
			for (int32 MaterialIndex = 0; MaterialIndex < PrimitiveComponent->GetNumMaterials(); ++MaterialIndex)
			{
				PrimitiveComponent->SetMaterial(MaterialIndex, FakeMoonBendMaterial);
			}
		}

		if (bApplyBendBoundsScale)
		{
			const float BaseBoundsRadius = FMath::Max(1.0f, PrimitiveComponent->Bounds.SphereRadius);
			PrimitiveComponent->SetBoundsScale(WorldConfig->GetRecommendedBoundsScale(BaseBoundsRadius));
		}
	}

	bRenderingSettingsApplied = true;
}
