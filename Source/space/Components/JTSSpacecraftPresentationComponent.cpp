// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Components/JTSSpacecraftPresentationComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/World/JTSPlanetLandingTypes.h"

UJTSSpacecraftPresentationComponent::UJTSSpacecraftPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// Pose and plume scale are cosmetic. The movement component still owns the ship transform.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UJTSSpacecraftPresentationComponent::BeginPlay()
{
	Super::BeginPlay();
	DiscoverRig();
	GearAlpha = GetDesiredGearAlpha();
	const float Eased = FMath::InterpEaseInOut(0.0f, 1.0f, GearAlpha, 2.0f);
	for (const FGearBinding& Leg : Gear)
	{
		ApplyGearPose(Leg, Eased);
	}
	SmoothedMainThrottle = GetCommandedMainThrottle();
	SmoothedLiftThrottle = GetCommandedLiftThrottle();
	for (FPlumeBinding& Plume : Plumes)
	{
		ApplyPlume(Plume, Plume.bIsMainNozzle ? SmoothedMainThrottle : SmoothedLiftThrottle);
	}
}

void UJTSSpacecraftPresentationComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AActor* const Owner = GetOwner();
	if (!IsValid(Owner) || Owner->IsActorBeingDestroyed())
	{
		bRigReady = false;
		return;
	}
	if (!bRigReady)
	{
		DiscoverRig();
	}
	UpdateGear(DeltaTime);
	UpdateExhaust(DeltaTime);
}

float UJTSSpacecraftPresentationComponent::GetGearDeployAlpha() const
{
	return GearAlpha;
}

void UJTSSpacecraftPresentationComponent::DiscoverRig()
{
	AActor* const Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	ReleaseExhaustCores();
	Gear.Reset();
	Plumes.Reset();

	TInlineComponentArray<USceneComponent*> SceneComponents(Owner);
	for (USceneComponent* const Component : SceneComponents)
	{
		if (!IsValid(Component) || Component == Owner->GetRootComponent())
		{
			continue;
		}

		const FString Name = Component->GetName();
		if (Name.StartsWith(TEXT("Gear_")))
		{
			FGearBinding& Leg = Gear.AddDefaulted_GetRef();
			Leg.Component = Component;
			FString LegName = Name;
			if (LegName.EndsWith(TEXT("_Shin")))
			{
				Leg.bIsShin = true;
				LegName.LeftChopInline(5);
			}
			else if (LegName.EndsWith(TEXT("_Foot")))
			{
				Leg.bIsFoot = true;
				LegName.LeftChopInline(5);
			}
			Leg.LegName = FName(*LegName);
			const FName GearName(*Name);
			if (const FVector* const SavedLocation = CapturedGearLocation.Find(GearName))
			{
				Leg.DeployedLocation = *SavedLocation;
				Leg.DeployedRotation = CapturedGearRotation.FindChecked(GearName);
				Leg.DeployedScale = CapturedGearScale.FindChecked(GearName);
			}
			else
			{
				Leg.DeployedLocation = Component->GetRelativeLocation();
				Leg.DeployedRotation = Component->GetRelativeRotation();
				Leg.DeployedScale = Component->GetRelativeScale3D();
				CapturedGearLocation.Add(GearName, Leg.DeployedLocation);
				CapturedGearRotation.Add(GearName, Leg.DeployedRotation);
				CapturedGearScale.Add(GearName, Leg.DeployedScale);
			}
			if (UPrimitiveComponent* const Primitive = Cast<UPrimitiveComponent>(Component))
			{
				Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}
		else if (UStaticMeshComponent* const MeshComponent = Cast<UStaticMeshComponent>(Component))
		{
			const bool bMain = Name.StartsWith(TEXT("Plume_Main"));
			const bool bLift = Name.StartsWith(TEXT("Plume_Lift"));
			if (!bMain && !bLift)
			{
				continue;
			}

			FPlumeBinding& Plume = Plumes.AddDefaulted_GetRef();
			Plume.Component = MeshComponent;
			Plume.bIsMainNozzle = bMain;
			// Plume_Main is the centre bell. Plume_Main_L and Plume_Main_R are the outboard bells.
			Plume.bIsCentreBell = bMain && !Name.Contains(TEXT("_L")) && !Name.Contains(TEXT("_R"));
			// The centre bell of the rear row carries the thrust. The two outboard bells
			// are shorter so the cluster reads as one plume with a bright core.
			const float LengthScale = Plume.bIsCentreBell ? 1.18f : (bMain ? 0.86f : 1.0f);
			const float RadiusScale = Plume.bIsCentreBell ? 1.15f : 1.0f;
			Plume.Length = (bMain ? MainPlumeLength : LiftPlumeLength) * LengthScale;
			Plume.Radius = (bMain ? MainPlumeRadius : LiftPlumeRadius) * RadiusScale;
			MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			MeshComponent->SetCastShadow(false);
			MeshComponent->SetVisibility(false);
			if (UMaterialInterface* const Material = MeshComponent->GetMaterial(0))
			{
				Plume.Material = MeshComponent->CreateDynamicMaterialInstance(0, Material);
			}
			EnsureExhaustCore(Plume);
		}
	}

	bRigReady = Gear.Num() > 0 || Plumes.Num() > 0;
}

void UJTSSpacecraftPresentationComponent::ReleaseExhaustCores()
{
	for (FPlumeBinding& Plume : Plumes)
	{
		if (UStaticMeshComponent* const Core = Plume.Core.Get())
		{
			Core->DestroyComponent();
		}
		Plume.Core = nullptr;
		if (UStaticMeshComponent* const Halo = Plume.Halo.Get())
		{
			Halo->DestroyComponent();
		}
		Plume.Halo = nullptr;
	}
}

void UJTSSpacecraftPresentationComponent::EnsureExhaustCore(FPlumeBinding& Plume)
{
	UStaticMeshComponent* const PlumeMesh = Plume.Component.Get();
	if (!IsValid(PlumeMesh) || Plume.Core.IsValid())
	{
		return;
	}

	UStaticMeshComponent* const Core = NewObject<UStaticMeshComponent>(PlumeMesh, NAME_None);
	if (!IsValid(Core))
	{
		return;
	}

	Core->SetupAttachment(PlumeMesh);
	Core->SetStaticMesh(PlumeMesh->GetStaticMesh());
	Core->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Core->SetCastShadow(false);
	Core->SetVisibility(false);
	Core->SetRelativeLocation(FVector::ZeroVector);
	Core->SetRelativeRotation(FRotator::ZeroRotator);
	Core->SetRelativeScale3D(FVector(0.62f, 0.30f, 0.30f));
	Core->RegisterComponent();

	UMaterialInterface* const Source = IsValid(Plume.Material)
		? static_cast<UMaterialInterface*>(Plume.Material.Get())
		: PlumeMesh->GetMaterial(0);
	if (IsValid(Source))
	{
		Plume.CoreMaterial = Core->CreateDynamicMaterialInstance(0, Source);
		if (IsValid(Plume.CoreMaterial))
		{
			// Sheath keeps the nozzle colour. The core is the white-hot centre of the same plume.
			const FLinearColor HotCore = Plume.bIsMainNozzle
				? FLinearColor(0.82f, 0.96f, 1.0f, 1.0f)
				: FLinearColor(1.0f, 0.94f, 0.72f, 1.0f);
			Plume.CoreMaterial->SetVectorParameterValue(TEXT("Tint"), HotCore);
		}
	}

	// A short shock diamond sits at the lip so the plume reads as leaving the bell, not as a floating cone.
	UStaticMeshComponent* const Halo = NewObject<UStaticMeshComponent>(PlumeMesh, NAME_None);
	if (IsValid(Halo))
	{
		Halo->SetupAttachment(PlumeMesh);
		Halo->SetStaticMesh(PlumeMesh->GetStaticMesh());
		Halo->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Halo->SetCastShadow(false);
		Halo->SetVisibility(false);
		Halo->SetRelativeLocation(FVector::ZeroVector);
		Halo->SetRelativeRotation(FRotator::ZeroRotator);
		Halo->SetRelativeScale3D(FVector(0.16f, 1.35f, 1.35f));
		Halo->RegisterComponent();
		if (IsValid(Source))
		{
			Plume.HaloMaterial = Halo->CreateDynamicMaterialInstance(0, Source);
			if (IsValid(Plume.HaloMaterial))
			{
				const FLinearColor Lip = Plume.bIsMainNozzle
					? FLinearColor(0.55f, 0.86f, 1.0f, 1.0f)
					: FLinearColor(1.0f, 0.62f, 0.18f, 1.0f);
				Plume.HaloMaterial->SetVectorParameterValue(TEXT("Tint"), Lip);
			}
		}
		Plume.Halo = Halo;
	}

	Plume.Core = Core;
}

float UJTSSpacecraftPresentationComponent::GetDesiredGearAlpha() const
{
	const AJTSSpacecraftActor* const Ship = Cast<AJTSSpacecraftActor>(GetOwner());
	if (!IsValid(Ship))
	{
		return 1.0f;
	}

	// Touchdown and a parked ship keep the feet down. The fold starts as soon as
	// the pilot leaves the surface, and the feet come back out for the whole descent.
	switch (Ship->GetFlightState())
	{
	case EJTSSpacecraftFlightState::Landed:
	case EJTSSpacecraftFlightState::LandingRequest:
	case EJTSSpacecraftFlightState::LandingAssist:
		return 1.0f;
	case EJTSSpacecraftFlightState::Flying:
	default:
		return 0.0f;
	}
}

float UJTSSpacecraftPresentationComponent::GetCommandedMainThrottle() const
{
	const AJTSSpacecraftActor* const Ship = Cast<AJTSSpacecraftActor>(GetOwner());
	return IsValid(Ship) ? Ship->GetPresentationMainThrottle() : 0.0f;
}

float UJTSSpacecraftPresentationComponent::GetCommandedLiftThrottle() const
{
	const AJTSSpacecraftActor* const Ship = Cast<AJTSSpacecraftActor>(GetOwner());
	return IsValid(Ship) ? Ship->GetPresentationLiftThrottle() : 0.0f;
}

void UJTSSpacecraftPresentationComponent::ApplyGearPose(const FGearBinding& Leg, float Eased) const
{
	USceneComponent* const GearComponent = Leg.Component.Get();
	if (!IsValid(GearComponent))
	{
		return;
	}

	// Deployed alpha is 1. The retract runs in two beats across the landing window:
	// the shin slides home first, then the upper leg folds aft until the pad sits in the bay.
	const float Fold = 1.0f - FMath::Clamp(Eased, 0.0f, 1.0f);
	const float Slide = FMath::Clamp(Fold / 0.42f, 0.0f, 1.0f);
	const float Hinge = FMath::Clamp((Fold - 0.28f) / 0.72f, 0.0f, 1.0f);
	const float SlideEase = FMath::InterpEaseInOut(0.0f, 1.0f, Slide, 2.0f);
	const float HingeEase = FMath::InterpEaseInOut(0.0f, 1.0f, Hinge, 2.0f);

	// Pitch folds the knee out past the side hull. Roll swings it toward the centreline,
	// and a small yaw tucks the pad into the fork gap instead of leaving it under the skin.
	const float SideSign = Leg.DeployedLocation.Y >= 0.0f ? -1.0f : 1.0f;
	const float YawSign = Leg.DeployedLocation.X >= 0.0f ? 1.0f : -1.0f;

	FVector Location = Leg.DeployedLocation;
	FRotator Rotation = Leg.DeployedRotation;
	if (Leg.bIsShin)
	{
		// The shin component sits on the knee. Its mesh runs another 34cm down to the ankle,
		// so sliding the component 28cm back along its own up-axis nests that strut in the thigh.
		const FVector Down = Leg.DeployedRotation.RotateVector(FVector(0.0f, 0.0f, -1.0f));
		Location += Down * FMath::Lerp(0.0f, -28.0f, SlideEase);
	}
	else if (!Leg.bIsFoot)
	{
		Rotation.Roll = Leg.DeployedRotation.Roll + SideSign * 88.0f * HingeEase;
		Rotation.Yaw = Leg.DeployedRotation.Yaw + YawSign * 22.0f * HingeEase;
	}

	GearComponent->SetRelativeLocation(Location);
	GearComponent->SetRelativeRotation(Rotation);
	GearComponent->SetRelativeScale3D(Leg.DeployedScale);
	GearComponent->SetVisibility(true, true);
}

void UJTSSpacecraftPresentationComponent::UpdateGear(float DeltaTime)
{
	const float Desired = GetDesiredGearAlpha();
	const float Duration = FMath::Max(0.05f, GearTransitionDuration);
	GearAlpha = FMath::FInterpConstantTo(GearAlpha, Desired, DeltaTime, 1.0f / Duration);

	for (const FGearBinding& Leg : Gear)
	{
		if (!Leg.Component.IsValid())
		{
			// A play session recompiled the ship out from under this tick. Drop the rig
			// and pick the new components up next frame instead of touching a freed one.
			bRigReady = false;
			Gear.Reset();
			return;
		}
	}

	const float Eased = FMath::InterpEaseInOut(0.0f, 1.0f, GearAlpha, 2.0f);
	for (const FGearBinding& Leg : Gear)
	{
		ApplyGearPose(Leg, Eased);
	}
}

void UJTSSpacecraftPresentationComponent::UpdateExhaust(float DeltaTime)
{
	const float Response = FMath::Max(0.1f, ExhaustResponse);
	const float Blend = 1.0f - FMath::Exp(-Response * DeltaTime);
	SmoothedMainThrottle = FMath::Lerp(SmoothedMainThrottle, GetCommandedMainThrottle(), Blend);
	SmoothedLiftThrottle = FMath::Lerp(SmoothedLiftThrottle, GetCommandedLiftThrottle(), Blend);

	// A little lengthwise flicker keeps a held key from looking like a frozen cone.
	const float Flicker = 0.92f + 0.08f * FMath::Sin(GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds() * 31.0f
		: 0.0f);

	for (FPlumeBinding& Plume : Plumes)
	{
		const float Strength = (Plume.bIsMainNozzle ? SmoothedMainThrottle : SmoothedLiftThrottle) * Flicker;
		ApplyPlume(Plume, Strength);
	}
}

void UJTSSpacecraftPresentationComponent::ApplyPlume(FPlumeBinding& Plume, float Strength)
{
	UStaticMeshComponent* const PlumeMesh = Plume.Component.Get();
	if (!IsValid(PlumeMesh))
	{
		bRigReady = false;
		return;
	}

	const float Clamped = FMath::Clamp(Strength, 0.0f, 1.2f);
	auto HideChild = [](UStaticMeshComponent* Child)
	{
		if (IsValid(Child))
		{
			Child->SetVisibility(false);
		}
	};
	if (Clamped < ExhaustVisibleThreshold)
	{
		PlumeMesh->SetVisibility(false);
		HideChild(Plume.Core.Get());
		HideChild(Plume.Halo.Get());
		return;
	}

	PlumeMesh->SetVisibility(true);
	const float Width = FMath::Lerp(0.72f, 1.35f, FMath::Clamp(Clamped, 0.0f, 1.0f));
	PlumeMesh->SetRelativeScale3D(FVector(
		Plume.Length * Clamped,
		Plume.Radius * Width,
		Plume.Radius * Width));
	if (IsValid(Plume.Material))
	{
		Plume.Material->SetScalarParameterValue(TEXT("Strength"), Clamped);
	}

	if (UStaticMeshComponent* const Core = Plume.Core.Get())
	{
		// Hot core rides inside the sheath. Its scale is relative to the sheath, so throttle
		// still owns the overall length.
		Core->SetVisibility(true);
		const float CoreLength = FMath::Lerp(0.62f, 0.84f, FMath::Clamp(Clamped, 0.0f, 1.0f));
		Core->SetRelativeScale3D(FVector(CoreLength, 0.34f, 0.34f));
		if (IsValid(Plume.CoreMaterial))
		{
			Plume.CoreMaterial->SetScalarParameterValue(TEXT("Strength"), FMath::Clamp(Clamped * 1.2f, 0.0f, 1.2f));
		}
	}

	if (UStaticMeshComponent* const Halo = Plume.Halo.Get())
	{
		Halo->SetVisibility(true);
		const float HaloWidth = FMath::Lerp(1.05f, 1.85f, FMath::Clamp(Clamped, 0.0f, 1.0f));
		Halo->SetRelativeScale3D(FVector(0.14f, HaloWidth, HaloWidth));
		if (IsValid(Plume.HaloMaterial))
		{
			Plume.HaloMaterial->SetScalarParameterValue(TEXT("Strength"), FMath::Clamp(Clamped * 0.85f, 0.0f, 1.2f));
		}
	}
}
