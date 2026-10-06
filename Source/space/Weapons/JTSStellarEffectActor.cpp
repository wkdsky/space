#include "space/Weapons/JTSStellarEffectActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
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
	for (int32 Index = 0; Index < 8; ++Index)
		ChainBeams.Add(CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("ChainBeam%d"), Index)));
	TArray<UStaticMeshComponent*> Meshes { Beam, Field, Repulsion, BeamGlow, Impact, Core, Orbit, OrbitInner };
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
	SetLifeSpan(0.40f);
	const double Now = GetWorld()->GetTimeSeconds();
	if (Mode == EJTSStellarWeaponMode::Focus && !bFromMuzzle)
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
	Core->SetVisibility(bPrimary && bField);
	Orbit->SetVisibility(false);
	OrbitInner->SetVisibility(false);
	Repulsion->SetVisibility(bSecondary && bField && bShowRepulsionBoundary);
	Impact->SetVisibility(bPrimary && !bField && bImpact);
	if (const AJTSCharacter* Character = Cast<AJTSCharacter>(GetOwner()))
		FieldUp = Character->GetGameplayPlanet() ? Character->GetGameplayPlanet()->GetRadialUpVector(End) : Character->GetActorUpVector();
	else FieldUp = GetOwner() ? GetOwner()->GetActorUpVector() : FVector::UpVector;
	Field->SetWorldLocation(End);
	Field->SetWorldScale3D(FVector(Radius / 50));
	Core->SetWorldLocation(End);
	Core->SetWorldScale3D(FVector(CurrentCoreRadius / 50));
	Impact->SetWorldLocation(End);
	Impact->SetWorldScale3D(FVector((Mode == EJTSStellarWeaponMode::Jet ? 55.0f : 32.0f) / 50));
	Repulsion->SetWorldLocation(Start);
	Repulsion->SetWorldScale3D(FVector(SecondaryRadius / 50));
	TArray<UStaticMeshComponent*> Meshes { Beam, Field, Repulsion, BeamGlow, Impact, Core, Orbit, OrbitInner };
	for (UStaticMeshComponent* Link : ChainBeams) Meshes.Add(Link);
	for (auto* Mesh : Meshes)
		if (Mesh->GetMaterial(0))
		{
			auto* Material = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
			if (!Material) Material = Mesh->CreateAndSetMaterialInstanceDynamic(0);
			Material->SetVectorParameterValue(TEXT("Color"), Color);
			Material->SetScalarParameterValue(TEXT("Fade"), 1.0f);
		}
	const bool bLoop = bPrimary && Mode != EJTSStellarWeaponMode::Focus;
	ChannelAudio->SetWorldLocation(bField ? End : Start);
	if (bLoop && LoopSound && !ChannelAudio->IsPlaying())
	{
		ChannelAudio->SetSound(LoopSound);
		ChannelAudio->FadeIn(0.08f, bField ? 0.12f : 0.35f);
	}
	else if (!bLoop && ChannelAudio->IsPlaying()) ChannelAudio->FadeOut(0.08f, 0.0f);
	if (bPrimary && Mode == EJTSStellarWeaponMode::Focus && ShotSound)
		UGameplayStatics::PlaySoundAtLocation(this, ShotSound, Start, 0.45f);
	if (bField && bSecondary && bShowRepulsionBoundary && RepulsionSound && Now - LastRepulsionSound >= 0.8)
	{
		UGameplayStatics::PlaySoundAtLocation(this, RepulsionSound, Start, 0.5f);
		LastRepulsionSound = Now;
	}
	Tick(0.0f);
	OnEffectUpdated(Mode, Start, End, Radius, Color, bPrimary, bSecondary);
}

void AJTSStellarEffectActor::UpdatePersistentField(FVector Center, FVector SurfaceUp, float Radius, float CoreRadius, FLinearColor Color)
{
	CurrentCoreRadius = FMath::Max(10.0f, CoreRadius);
	UpdateEffect(EJTSStellarWeaponMode::BlackHole, Center, Center, Radius, Color, true, false);
	SetLifeSpan(0.0f);
	FieldUp = SurfaceUp.GetSafeNormal();
	Tick(0.0f);
}

void AJTSStellarEffectActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Time = GetWorld()->GetTimeSeconds();
	const float Age = Time - LastMainUpdate;
	if (ActiveMode != EJTSStellarWeaponMode::BlackHole)
	{
		FVector Start = Source;
		if (const auto* Visual = GetOwner() ? GetOwner()->FindComponentByClass<UJTSWeaponVisualComponent>() : nullptr)
			Visual->GetMuzzleWorldLocation(Start);
		const bool bCone = ActiveMode == EJTSStellarWeaponMode::Jet;
		SetBeamTransform(Beam, Start, Destination, bCone ? FieldRadius : FieldRadius * 0.35f, bCone);
		SetBeamTransform(BeamGlow, Start, Destination, bCone ? FieldRadius * 0.62f : FieldRadius, bCone);
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
	const float Fade = ActiveMode == EJTSStellarWeaponMode::Focus ? FMath::Clamp(1 - Age / 0.13f, 0.0f, 1.0f) : 1.0f;
	for (auto* Mesh : { Beam.Get(), BeamGlow.Get(), Impact.Get() })
		if (auto* Material = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)))
			Material->SetScalarParameterValue(TEXT("Fade"), Fade);
	for (UStaticMeshComponent* Link : ChainBeams)
		if (auto* Material = Cast<UMaterialInstanceDynamic>(Link->GetMaterial(0)))
			Material->SetScalarParameterValue(TEXT("Fade"), Fade);
}
