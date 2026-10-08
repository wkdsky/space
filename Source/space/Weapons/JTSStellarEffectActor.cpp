#include "space/Weapons/JTSStellarEffectActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RotationMatrix.h"
#include "space/Components/JTSWeaponVisualComponent.h"
#include "space/Player/JTSCharacter.h"
#include "space/World/JTSPlanetAnchor.h"

AJTSStellarEffectActor::AJTSStellarEffectActor()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorEnableCollision(false);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Beam = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beam"));
	Field = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Field"));
	Repulsion = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Repulsion"));
	BeamGlow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeamGlow"));
	Impact = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Impact"));
	Core = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Core"));
	Orbit = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Orbit"));
	OrbitInner = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OrbitInner"));
	Hailstones = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Hailstones"));
	HailTrails = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HailTrails"));
	HailImpacts = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HailImpacts"));
	for (int32 Index = 0; Index < 8; ++Index)
		ChainBeams.Add(CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("ChainBeam%d"), Index)));
	TArray<UStaticMeshComponent*> Meshes { Beam, Field, Repulsion, BeamGlow, Impact, Core, Orbit, OrbitInner, Hailstones, HailTrails, HailImpacts };
	for (UStaticMeshComponent* Mesh : ChainBeams) Meshes.Add(Mesh);
	for (auto* Mesh : Meshes)
	{
		Mesh->SetupAttachment(GetRootComponent());
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCastShadow(false);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetVisibility(false);
	}
	ChannelAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("ChannelAudio"));
	ChannelAudio->SetupAttachment(GetRootComponent());
	ChannelAudio->bAutoActivate = false;
}

void AJTSStellarEffectActor::SetBeamTransform(UStaticMeshComponent* Mesh, FVector Start, FVector End, float Width, bool bCone)
{
	const FVector Delta = End - Start;
	Mesh->SetWorldLocation(bCone ? Start : (Start + End) * 0.5f);
	Mesh->SetWorldRotation(FRotationMatrix::MakeFromZ(Delta.GetSafeNormal()).Rotator());
	// Blueprint selects a unit cone (base radius 50, length 100) or a unit cylinder.
	Mesh->SetWorldScale3D(FVector(FMath::Max(0.01f, Width / 50), FMath::Max(0.01f, Width / 50), Delta.Size() / 100));
}

void AJTSStellarEffectActor::UpdateEffect(EJTSStellarWeaponMode Mode, FVector Start, FVector End,
	float Radius, FLinearColor Color, bool bPrimary, bool bSecondary, float SecondaryRadius, bool bFromMuzzle, bool bImpact)
{
	const bool bKeepAreaAudio = bAreaVisual && ChannelAudio->IsPlaying();
	SetLifeSpan(0.40f);
	const bool bKeepDrone = bDroneVisual;
	bAreaVisual = false; bFollowOwner = bKeepDrone; bBurstVisual = false;
	const double Now = GetWorld()->GetTimeSeconds();
	if ((Mode == EJTSStellarWeaponMode::Focus || Mode == EJTSStellarWeaponMode::Diffusion || Mode == EJTSStellarWeaponMode::Disassembly) && !bFromMuzzle)
	{
		UStaticMeshComponent* Link = ChainBeams[ChainIndex++ % ChainBeams.Num()];
		Link->SetVisibility(true);
		SetBeamTransform(Link, Start, End, FMath::Max(3.0f, Radius * 0.5f), false);
		return;
	}
	ActiveMode = Mode;
	Source = Start;
	Destination = End;
	FieldRadius = Radius;
	CurrentSecondaryRadius = SecondaryRadius;
	LastMainUpdate = Now;
	ChainIndex = 0;
	for (UStaticMeshComponent* Link : ChainBeams) Link->SetVisibility(false);
	const bool bField = Mode == EJTSStellarWeaponMode::BlackHole;
	Beam->SetVisibility(bPrimary && !bField);
	BeamGlow->SetVisibility(bPrimary && !bField);
	Field->SetVisibility(false);
	Core->SetVisibility(bPrimary && (bField || bFromMuzzle || bKeepDrone));
	Orbit->SetVisibility(bKeepDrone || (bPrimary && !bField && bFromMuzzle));
	OrbitInner->SetVisibility(bKeepDrone);
	Repulsion->SetVisibility(bSecondary && bField && bShowRepulsionBoundary);
	Hailstones->SetVisibility(false); HailTrails->SetVisibility(false); HailImpacts->SetVisibility(false);
	Impact->SetVisibility(bPrimary && !bField && bImpact);
	if (const AJTSCharacter* Character = Cast<AJTSCharacter>(GetOwner()))
		FieldUp = Character->GetGameplayPlanet() ? Character->GetGameplayPlanet()->GetRadialUpVector(End) : Character->GetActorUpVector();
	else FieldUp = GetOwner() ? GetOwner()->GetActorUpVector() : FVector::UpVector;
	Field->SetWorldLocation(End);
	Field->SetWorldScale3D(FVector(Radius / 50));
	Core->SetWorldLocation(bField ? End : Start);
	Core->SetWorldScale3D(FVector((bField ? CurrentCoreRadius : bKeepDrone ? 24.f : 16.f) / 50));
	Impact->SetWorldLocation(End);
	Impact->SetWorldScale3D(FVector((Mode == EJTSStellarWeaponMode::Jet ? 55.0f : 32.0f) / 50));
	Repulsion->SetWorldLocation(Start);
	Repulsion->SetWorldScale3D(FVector(SecondaryRadius / 50));
	TArray<UStaticMeshComponent*> Meshes { Beam, Field, Repulsion, BeamGlow, Impact, Core, Orbit, OrbitInner, Hailstones, HailTrails, HailImpacts };
	for (UStaticMeshComponent* Link : ChainBeams) Meshes.Add(Link);
	for (auto* Mesh : Meshes)
		if (Mesh->GetMaterial(0))
		{
			auto* Material = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
			if (!Material) Material = Mesh->CreateAndSetMaterialInstanceDynamic(0);
			Material->SetVectorParameterValue(TEXT("Color"), Color);
			Material->SetScalarParameterValue(TEXT("Fade"), 1.0f);
		}
	const bool bLoop = bPrimary && (Mode == EJTSStellarWeaponMode::Jet || Mode == EJTSStellarWeaponMode::BlackHole || Mode == EJTSStellarWeaponMode::Disassembly);
	ChannelAudio->SetWorldLocation(bField ? End : Start);
	if (bLoop && LoopSound && !ChannelAudio->IsPlaying())
	{
		ChannelAudio->SetSound(LoopSound);
		ChannelAudio->FadeIn(0.08f, bField ? 0.12f : 0.35f);
	}
	else if (!bLoop && !bKeepAreaAudio && ChannelAudio->IsPlaying()) ChannelAudio->FadeOut(0.08f, 0.0f);
	if (bPrimary && (Mode == EJTSStellarWeaponMode::Focus || Mode == EJTSStellarWeaponMode::Freezing || Mode == EJTSStellarWeaponMode::Diffusion || Mode == EJTSStellarWeaponMode::Instance) && ShotSound)
		UGameplayStatics::PlaySoundAtLocation(this, ShotSound, Start, 0.45f);
	if (bField && bSecondary && bShowRepulsionBoundary && RepulsionSound && Now - LastRepulsionSound >= 0.8)
	{
		UGameplayStatics::PlaySoundAtLocation(this, RepulsionSound, Start, 0.5f);
		LastRepulsionSound = Now;
	}
	Tick(0.0f);
	OnEffectUpdated(Mode, Start, End, Radius, Color, bPrimary, bSecondary);
	if (bKeepDrone) SetLifeSpan(0);
}

void AJTSStellarEffectActor::UpdateArea(EJTSStellarWeaponMode Mode, FVector Center, FVector SurfaceUp, float Radius,
	FLinearColor Color, bool Persistent, bool Shield, bool Burst, float SweepAngle)
{
	const bool WasArea = bAreaVisual;
	UpdateEffect(Mode, Center, Center, Radius, Color, false, false);
	bAreaVisual = true; bShieldVisual = Shield; bBurstVisual = Burst; SweepDegrees = SweepAngle;
	FieldUp = SurfaceUp.GetSafeNormal(); Source = Center; Destination = Center;
	SetLifeSpan(Persistent ? 0.f : Shield && Mode != EJTSStellarWeaponMode::Shaping ? 5.f : Burst ? .6f : .28f);
	Field->SetVisibility(!Shield || Mode == EJTSStellarWeaponMode::Shaping);
	Orbit->SetVisibility(!Shield || Mode == EJTSStellarWeaponMode::Shaping);
	Repulsion->SetVisibility(Shield && Mode != EJTSStellarWeaponMode::Shaping);
	if (Mode == EJTSStellarWeaponMode::Explosion && Burst) Core->SetVisibility(true);
	const bool Hail = Mode == EJTSStellarWeaponMode::Freezing && !Burst && !Shield;
	if (!Burst && !Hail && Cast<AJTSCharacter>(GetOwner())) { Core->SetVisibility(true); OrbitInner->SetVisibility(true); }
	Hailstones->SetVisibility(Hail); HailTrails->SetVisibility(Hail); HailImpacts->SetVisibility(Hail);
	if (Burst && ShotSound) UGameplayStatics::PlaySoundAtLocation(this, ShotSound, Center, .4f);
	if (!Burst && LoopSound && !ChannelAudio->IsPlaying())
	{
		ChannelAudio->SetSound(LoopSound); ChannelAudio->SetWorldLocation(Center); ChannelAudio->FadeIn(.08f,.25f);
	}
	else if (!Burst && !WasArea && ShotSound) UGameplayStatics::PlaySoundAtLocation(this, ShotSound, Center, .25f);
	Tick(0);
}
void AJTSStellarEffectActor::UpdateProjectile(EJTSStellarWeaponMode Mode, FVector Center, FLinearColor Color)
{
	UpdateEffect(Mode, Center, Center, 20, Color, false, false);
	bFollowOwner = true; Core->SetVisibility(true); Orbit->SetVisibility(true); Core->SetWorldScale3D(FVector(.4)); SetLifeSpan(0);
}
void AJTSStellarEffectActor::UpdateDrone(FVector Center, FLinearColor Color)
{
	UpdateProjectile(EJTSStellarWeaponMode::Instance, Center, Color); bDroneVisual = true;
	Core->SetWorldScale3D(FVector(.48)); OrbitInner->SetVisibility(true);
}
void AJTSStellarEffectActor::UpdateOwnerShield(float Radius, FLinearColor Color)
{
	const FVector Center = GetOwner() ? GetOwner()->GetActorLocation() : GetActorLocation();
	UpdateEffect(EJTSStellarWeaponMode::Shadow, Center, Center, Radius, Color, false, false);
	bOwnerShieldVisual = true; bFollowOwner = true; CurrentSecondaryRadius = Radius;
	Repulsion->SetVisibility(true);
	SetActorTickInterval(.1f); SetLifeSpan(0); Tick(0);
}

void AJTSStellarEffectActor::UpdatePersistentField(FVector Center, FVector SurfaceUp, float Radius, float CoreRadius, FLinearColor Color)
{
	CurrentCoreRadius = FMath::Max(10.0f, CoreRadius);
	UpdateEffect(EJTSStellarWeaponMode::BlackHole, Center, Center, Radius, Color, true, false);
	SetLifeSpan(0.0f);
	FieldUp = SurfaceUp.GetSafeNormal();
	Tick(0.0f);
}

void AJTSStellarEffectActor::UpdateHail(float Time, const FQuat& Rotation)
{
	TArray<FTransform> Stones, Trails, Impacts;
	const int32 Count = FMath::Clamp(HailCount, 8, 48);
	const float Height = FMath::Max(300.f, HailHeight);
	const float Speed = FMath::Max(300.f, HailFallSpeed);
	const float Period = Height / Speed + .12f;
	if (HailCycles.Num() != Count)
	{
		HailCycles.Init(-1, Count); HailLandingPositions.SetNum(Count);
	}
	for (int32 I = 0; I < Count; ++I)
	{
		// Golden-angle spacing covers the disc; staggered cycles read as rainfall.
		const float Angle = I * 2.399963f;
		const float Radius = FieldRadius * FMath::Sqrt((I + .5f) / Count) * .96f;
		const FVector Sample = Destination + Rotation.RotateVector(FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0));
		const float Clock = Time + I * Period / Count;
		const int64 Cycle = FMath::FloorToInt64(Clock / Period);
		if (HailCycles[I] != Cycle)
		{
			HailCycles[I] = Cycle;
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(StellarHailSurface), false, GetOwner());
			HailLandingPositions[I] = GetWorld()->LineTraceSingleByObjectType(Hit, Sample + FieldUp * Height,
				Sample - FieldUp * 300, FCollisionObjectQueryParams(ECC_WorldStatic), Params) ? Hit.ImpactPoint : Sample;
		}
		const FVector Landing = HailLandingPositions[I];
		const float Phase = FMath::Fmod(Clock, Period);
		const float AboveGround = FMath::Max(0.f, Height - Phase * Speed);
		const bool Falling = AboveGround > 0;
		const FVector Position = Landing + FieldUp * AboveGround;
		Stones.Emplace(Rotation, Position, Falling ? FVector(.16f, .16f, .38f) : FVector::ZeroVector);
		Trails.Emplace(Rotation, Position + FieldUp * 45, Falling ? FVector(.045f, .045f, .9f) : FVector::ZeroVector);
		const float ImpactAge = FMath::Clamp((Phase - Height / Speed) / .12f, 0.f, 1.f);
		const float ImpactSize = 10 + ImpactAge * 42;
		Impacts.Emplace(Rotation, Landing + FieldUp * 3,
			Falling ? FVector::ZeroVector : FVector(ImpactSize / 50, ImpactSize / 50, .03f));
	}
	const auto Update = [](UInstancedStaticMeshComponent* Mesh, const TArray<FTransform>& Transforms)
	{
		if (Mesh->GetInstanceCount() != Transforms.Num()) { Mesh->ClearInstances(); Mesh->AddInstances(Transforms, false, true); }
		else Mesh->BatchUpdateInstancesTransforms(0, Transforms, true, true, true);
	};
	Update(Hailstones, Stones); Update(HailTrails, Trails); Update(HailImpacts, Impacts);
}

void AJTSStellarEffectActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Time = GetWorld()->GetTimeSeconds();
	const float Age = Time - LastMainUpdate;
	if (bFollowOwner && GetOwner())
	{
		const FVector Center = GetOwner()->GetActorLocation();
		if (bOwnerShieldVisual)
		{
			Repulsion->SetWorldLocation(Center); Repulsion->SetWorldScale3D(FVector(CurrentSecondaryRadius / 50));
			return;
		}
		Core->SetWorldLocation(Center); Orbit->SetWorldLocation(Center); OrbitInner->SetWorldLocation(Center);
		const FVector Up = GetOwner()->GetActorUpVector();
		Orbit->SetWorldRotation((FQuat(Up, Time * 2) * FRotationMatrix::MakeFromZ(Up).ToQuat()).Rotator());
		Orbit->SetWorldScale3D(FVector(.7, .7, .7));
		OrbitInner->SetWorldRotation((FQuat(Up, -Time * 3) * FRotationMatrix::MakeFromX(Up).ToQuat()).Rotator());
		OrbitInner->SetWorldScale3D(FVector(.55));
		if (bDroneVisual && Beam->IsVisible())
		{
			SetBeamTransform(Beam, Center, Destination, 2, false); SetBeamTransform(BeamGlow, Center, Destination, 4, false);
			Beam->SetVisibility(Age < .12f); BeamGlow->SetVisibility(Age < .12f); Impact->SetVisibility(Age < .12f);
		}
		return;
	}
	if (bAreaVisual)
	{
		const bool CasterField = !bBurstVisual && (ActiveMode == EJTSStellarWeaponMode::Radiance || ActiveMode == EJTSStellarWeaponMode::Shadow || ActiveMode == EJTSStellarWeaponMode::Freezing);
		if (CasterField) if (const auto* Character = Cast<AJTSCharacter>(GetOwner()))
		{
			FieldUp = Character->GetGameplayPlanet() ? Character->GetGameplayPlanet()->GetRadialUpVector(Character->GetActorLocation()) : Character->GetActorUpVector();
			Destination = Character->GetActorLocation() - FieldUp * Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + FieldUp * 5;
		}
		FVector Facing = GetOwner() ? GetOwner()->GetActorForwardVector() : FVector::ForwardVector;
		const FQuat Rotation = FRotationMatrix::MakeFromZX(FieldUp, Facing).ToQuat();
		const bool Explosion = ActiveMode == EJTSStellarWeaponMode::Explosion && bBurstVisual;
		const float Grow = Explosion ? FMath::Clamp(Age / .18f, .1f, 1.f) : 1.f;
		Field->SetWorldLocation(Destination + FieldUp * 3); Field->SetWorldRotation(Rotation);
		Field->SetWorldScale3D(FVector(FieldRadius / 50 * Grow, FieldRadius / 50 * Grow, Explosion ? FieldRadius / 50 * Grow : .035));
		Orbit->SetWorldLocation(Destination + FieldUp * 6); Orbit->SetWorldRotation(Rotation);
		Orbit->SetWorldScale3D(FVector(FieldRadius / 50, FieldRadius / 50, .15f));
		Repulsion->SetWorldLocation(Destination); Repulsion->SetWorldRotation(Rotation); Repulsion->SetWorldScale3D(FVector(FieldRadius / 50));
		Core->SetWorldLocation(Destination); Core->SetWorldScale3D(FVector(FieldRadius / 50 * Grow * .45f));
		if (!bBurstVisual && Core->IsVisible())
		{
			FVector Muzzle = Source;
			if (const auto* Visual = GetOwner() ? GetOwner()->FindComponentByClass<UJTSWeaponVisualComponent>() : nullptr) Visual->GetMuzzleWorldLocation(Muzzle);
			Core->SetWorldLocation(Muzzle); Core->SetWorldScale3D(FVector(.3f));
			OrbitInner->SetWorldLocation(Muzzle); OrbitInner->SetWorldScale3D(FVector(.55f));
			OrbitInner->SetWorldRotation((FQuat(FieldUp, Time * 2) * FRotationMatrix::MakeFromZ(FieldUp).ToQuat()).Rotator());
		}
		if (Hailstones->IsVisible()) UpdateHail(Time, Rotation);
		const float Fade = bBurstVisual ? FMath::Clamp(1 - Age / .6f, 0.f, 1.f) : 1.f;
		for (auto* Mesh : { Field.Get(), Orbit.Get(), Core.Get(), Repulsion.Get() }) if (auto* Mat = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)))
		{
			Mat->SetScalarParameterValue(TEXT("Fade"), Fade); Mat->SetScalarParameterValue(TEXT("SweepFraction"), SweepDegrees / 360.f);
		}
		return;
	}
	if (ActiveMode != EJTSStellarWeaponMode::BlackHole)
	{
		FVector Start = Source;
		if (const auto* Visual = GetOwner() ? GetOwner()->FindComponentByClass<UJTSWeaponVisualComponent>() : nullptr)
			Visual->GetMuzzleWorldLocation(Start);
		const bool bCone = ActiveMode == EJTSStellarWeaponMode::Jet;
		SetBeamTransform(Beam, Start, Destination, bCone ? FieldRadius : FieldRadius * 0.35f, bCone);
		SetBeamTransform(BeamGlow, Start, Destination, bCone ? FieldRadius * 0.62f : FieldRadius, bCone);
		if (Core->IsVisible()) Core->SetWorldLocation(Start);
		if (Orbit->IsVisible())
		{
			Orbit->SetWorldLocation(Start); Orbit->SetWorldRotation((FQuat(FieldUp, Time * 2) * FRotationMatrix::MakeFromZ(FieldUp).ToQuat()).Rotator());
			Orbit->SetWorldScale3D(FVector(.48));
		}
	}
	else
	{
		const FQuat SurfaceRotation = FRotationMatrix::MakeFromZ(FieldUp).ToQuat();
		Core->SetWorldRotation((FQuat(FieldUp, Time * 0.12f) * SurfaceRotation).Rotator());
		if (Repulsion->IsVisible())
		{
			FVector Ground = Source;
			if (const AJTSCharacter* Character = Cast<AJTSCharacter>(GetOwner()))
			{
				FieldUp = Character->GetGameplayPlanet() ? Character->GetGameplayPlanet()->GetRadialUpVector(Character->GetActorLocation()) : Character->GetActorUpVector();
				Ground = Character->GetActorLocation() - FieldUp * Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			}
			Repulsion->SetWorldLocation(Ground + FieldUp * 3);
			Repulsion->SetWorldRotation(FRotationMatrix::MakeFromZ(FieldUp).ToQuat());
			Repulsion->SetWorldScale3D(FVector(CurrentSecondaryRadius / 50));
		}
	}
	const bool Pulse = ActiveMode == EJTSStellarWeaponMode::Focus || ActiveMode == EJTSStellarWeaponMode::Freezing || ActiveMode == EJTSStellarWeaponMode::Diffusion;
	const float Fade = Pulse ? FMath::Clamp(1 - Age / 0.13f, 0.0f, 1.0f) : 1.0f;
	for (auto* Mesh : { Beam.Get(), BeamGlow.Get(), Impact.Get() })
		if (auto* Material = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)))
			Material->SetScalarParameterValue(TEXT("Fade"), Fade);
	for (UStaticMeshComponent* Link : ChainBeams)
		if (auto* Material = Cast<UMaterialInstanceDynamic>(Link->GetMaterial(0)))
			Material->SetScalarParameterValue(TEXT("Fade"), Fade);
}
