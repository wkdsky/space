// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Weapons/JTSGrenadeProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Weapons/JTSProjectileImpactActor.h"
#include "space/World/JTSMoonResourceActor.h"

AJTSGrenadeProjectile::AJTSGrenadeProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(true);
	SetNetUpdateFrequency(60.0f);
	InitialLifeSpan = 6.0f;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(9.0f);
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Collision->SetNotifyRigidBodyCollision(true);
	SetRootComponent(Collision);

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Collision);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCastShadow(false);
	Body->SetRelativeScale3D(FVector(0.18f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded()) Body->SetStaticMesh(Sphere.Object);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->bRotationFollowsVelocity = true;
	Movement->bShouldBounce = false;
	Movement->ProjectileGravityScale = 1.0f;
}

void AJTSGrenadeProjectile::BeginPlay()
{
	Super::BeginPlay();
	// The shooter must not detonate the shell on their own capsule as it leaves the muzzle.
	if (const APawn* const Shooter = GetInstigator()) Collision->IgnoreActorWhenMoving(const_cast<APawn*>(Shooter), true);
	Collision->OnComponentHit.AddDynamic(this, &AJTSGrenadeProjectile::HandleImpact);
	OnRep_Color();
}

void AJTSGrenadeProjectile::Launch(const FVector& Direction, float Speed, float GravityScale, float InDirectDamage,
	float InBlastRadius, int32 InFragments, float InFragmentDamage, float InMiningWork, EJTSItemId InSourceItem,
	const FLinearColor& InColor)
{
	DirectDamage = InDirectDamage;
	BlastRadius = InBlastRadius;
	Fragments = InFragments;
	FragmentDamage = InFragmentDamage;
	MiningWork = InMiningWork;
	SourceItem = InSourceItem;
	Color = InColor;
	Movement->ProjectileGravityScale = GravityScale;
	Movement->InitialSpeed = Speed;
	Movement->MaxSpeed = Speed;
	Movement->Velocity = Direction.GetSafeNormal() * Speed;
	Movement->UpdateComponentVelocity();
	OnRep_Color();
}

void AJTSGrenadeProjectile::OnRep_Color()
{
	if (UMaterialInstanceDynamic* const Material = Body->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), Color);
		Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
		Material->SetVectorParameterValue(TEXT("EmissiveColor"), Color * 2.0f);
	}
}

void AJTSGrenadeProjectile::HandleImpact(UPrimitiveComponent*, AActor*, UPrimitiveComponent*, FVector, const FHitResult& Hit)
{
	if (HasAuthority()) Detonate(Hit.ImpactPoint.IsNearlyZero() ? GetActorLocation() : FVector(Hit.ImpactPoint));
}

void AJTSGrenadeProjectile::Detonate(const FVector& Location)
{
	if (bDetonated || !HasAuthority() || GetWorld() == nullptr) return;
	bDetonated = true;
	AController* const InstigatorController = GetInstigator() != nullptr ? GetInstigator()->GetController() : nullptr;

	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByChannel(Overlaps, Location, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeSphere(BlastRadius));
	GetWorld()->OverlapMultiByChannel(Overlaps, Location, FQuat::Identity, ECC_WorldDynamic,
		FCollisionShape::MakeSphere(BlastRadius));
	TSet<AActor*> Handled;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* const Target = Overlap.GetActor();
		if (!IsValid(Target) || Target == GetInstigator() || Handled.Contains(Target)) continue;
		Handled.Add(Target);
		FVector Closest = Target->GetActorLocation();
		if (const UPrimitiveComponent* const Component = Overlap.GetComponent())
		{
			Component->GetClosestPointOnCollision(Location, Closest);
		}
		// Full damage at the centre, half at the rim. Line of sight keeps the blast from passing through walls.
		const float Distance = FVector::Dist(Location, Closest);
		const float Falloff = 1.0f - 0.5f * FMath::Clamp(Distance / FMath::Max(1.0f, BlastRadius), 0.0f, 1.0f);
		FHitResult LineOfSight;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(JTSGrenadeBlast), false, this);
		Params.AddIgnoredActor(Target);
		if (GetWorld()->LineTraceSingleByChannel(LineOfSight, Location + FVector(0, 0, 8), Closest, ECC_Visibility, Params)) continue;

		if (AJTSMoonResourceActor* const Resource = Cast<AJTSMoonResourceActor>(Target))
		{
			if (APawn* const Miner = GetInstigator()) Resource->ApplyMiningWork(Miner, SourceItem, MiningWork * Falloff);
		}
		else if (Target->FindComponentByClass<UJTSHealthComponent>() != nullptr)
		{
			UGameplayStatics::ApplyDamage(Target, DirectDamage * Falloff, InstigatorController, GetInstigator(),
				UDamageType::StaticClass());
		}
	}

	// Skill-granted fragments share their own budget, fired in a ring at roughly waist height.
	for (int32 Index = 0; Index < Fragments; ++Index)
	{
		const float Angle = 2.0f * PI * (static_cast<float>(Index) + FMath::FRand()) / static_cast<float>(Fragments);
		const FVector Direction(FMath::Cos(Angle), FMath::Sin(Angle), 0.15f);
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(JTSGrenadeFragment), false, this);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Location + FVector(0, 0, 40), Location + FVector(0, 0, 40) + Direction * BlastRadius * 1.4f,
			ECC_Visibility, Params))
		{
			AActor* const Target = Hit.GetActor();
			if (IsValid(Target) && Target != GetInstigator() && !Handled.Contains(Target)
				&& Target->FindComponentByClass<UJTSHealthComponent>() != nullptr)
			{
				Handled.Add(Target);
				UGameplayStatics::ApplyPointDamage(Target, FragmentDamage, Direction, Hit, InstigatorController, GetInstigator(),
					UDamageType::StaticClass());
			}
		}
	}

	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AJTSProjectileImpactActor* const Flash = GetWorld()->SpawnActor<AJTSProjectileImpactActor>(
		AJTSProjectileImpactActor::StaticClass(), FTransform(FRotator::ZeroRotator, Location), Spawn))
	{
		Flash->InitializeImpact(FVector::UpVector, nullptr, Color, false);
		Flash->SetActorScale3D(FVector(FMath::Clamp(BlastRadius / 120.0f, 1.0f, 6.0f)));
	}
	Destroy();
}

void AJTSGrenadeProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AJTSGrenadeProjectile, Color);
}
