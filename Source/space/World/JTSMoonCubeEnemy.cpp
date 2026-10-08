#include "space/World/JTSMoonCubeEnemy.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Net/UnrealNetwork.h"
#include "space/Components/JTSCriticalDamageType.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSStellarTargetComponent.h"
#include "space/Player/JTSCharacter.h"
#include "space/Systems/JTSPlanetEnemySubsystem.h"
#include "space/UI/JTSFloatingDamageActor.h"
#include "space/UI/JTSHealthBarWidget.h"
#include "space/World/JTSPlanetAnchor.h"
#include "TimerManager.h"

AJTSMoonCubeEnemy::AJTSMoonCubeEnemy()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(30.0f);

	BodyCollider = CreateDefaultSubobject<UBoxComponent>(TEXT("BodyCollider"));
	SetRootComponent(BodyCollider);
	BodyCollider->InitBoxExtent(FVector(45.0f));
	BodyCollider->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BodyCollider->SetCollisionObjectType(ECC_WorldDynamic);
	BodyCollider->SetCollisionResponseToAllChannels(ECR_Ignore);
	BodyCollider->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	BodyCollider->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	BodyCollider->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	BodyCollider->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	BodyCollider->SetGenerateOverlapEvents(false);
	BodyCollider->SetCanEverAffectNavigation(false);

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(BodyCollider);
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CubeBody"));
	BodyMesh->SetupAttachment(VisualRoot);
	BodyMesh->SetRelativeScale3D(FVector(0.9f));
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetCanEverAffectNavigation(false);

	WeakPointCollider = CreateDefaultSubobject<UBoxComponent>(TEXT("WeakPointCollider"));
	WeakPointCollider->SetupAttachment(VisualRoot);
	WeakPointCollider->SetRelativeLocation(FVector(0.0f, 0.0f, 62.0f));
	WeakPointCollider->InitBoxExtent(FVector(18.0f, 18.0f, 14.0f));
	WeakPointCollider->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	WeakPointCollider->SetCollisionObjectType(ECC_WorldDynamic);
	WeakPointCollider->SetCollisionResponseToAllChannels(ECR_Ignore);
	WeakPointCollider->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	WeakPointCollider->SetGenerateOverlapEvents(false);
	WeakPointCollider->SetCanEverAffectNavigation(false);

	WeakPointMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeakPointCrown"));
	WeakPointMesh->SetupAttachment(WeakPointCollider);
	WeakPointMesh->SetRelativeScale3D(FVector(0.34f, 0.34f, 0.25f));
	WeakPointMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeakPointMesh->SetCanEverAffectNavigation(false);

	HealthComponent = CreateDefaultSubobject<UJTSHealthComponent>(TEXT("Health"));
	CreateDefaultSubobject<UJTSStellarTargetComponent>(TEXT("StellarTarget"));
	HealthBar = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadHealth"));
	HealthBar->SetupAttachment(VisualRoot);
	HealthBar->SetRelativeLocation(FVector(0.0f, 0.0f, 106.0f));
	HealthBar->SetWidgetClass(UJTSHealthBarWidget::StaticClass());
	HealthBar->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBar->SetDrawSize(FVector2D(110.0f, 14.0f));
	HealthBar->SetPivot(FVector2D(0.5f, 1.0f));
	HealthBar->SetAbsolute(false, false, true);
	HealthBar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealthBar->SetGenerateOverlapEvents(false);
	HealthBar->SetVisibility(false);
	HealthBar->SetHiddenInGame(true);
}

void AJTSMoonCubeEnemy::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(!HasAuthority());
	HealthBar->InitWidget();
	HideHealthBar();
	if (HasAuthority())
	{
		HealthComponent->SetMaxHealth(MaxHealth, true);
		HealthComponent->OnDamaged.AddDynamic(this, &AJTSMoonCubeEnemy::HandleDamaged);
		HealthComponent->OnDeath.AddDynamic(this, &AJTSMoonCubeEnemy::HandleDeath);
	}
}

void AJTSMoonCubeEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(HealthComponent))
	{
		HealthComponent->OnDamaged.RemoveDynamic(this, &AJTSMoonCubeEnemy::HandleDamaged);
		HealthComponent->OnDeath.RemoveDynamic(this, &AJTSMoonCubeEnemy::HandleDeath);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HealthBarTimer);
		World->GetTimerManager().ClearTimer(VisualPulseTimer);
		if (HasAuthority() && EnemyEntity.IsValid())
		{
			if (UJTSPlanetEnemySubsystem* Enemies = World->GetSubsystem<UJTSPlanetEnemySubsystem>())
			{
				Enemies->UnregisterEnemy(EnemyEntity);
			}
			EnemyEntity = FMassEntityHandle();
		}
	}
	Super::EndPlay(EndPlayReason);
}

bool AJTSMoonCubeEnemy::InitializeForSettlement_Implementation(AJTSPlanetAnchor* InPlanet,
	FVector HomeLocation, FVector GroundLocation)
{
	if (!HasAuthority() || !IsValid(InPlanet)) return false;
	if (EnemyEntity.IsValid())
	{
		FJTSPlanetEnemyBodyFragment Body;
		auto* Enemies = GetWorld()->GetSubsystem<UJTSPlanetEnemySubsystem>();
		if (Enemies && Enemies->GetBodyCollisionData(EnemyEntity, Body)) return false;
		// An explicitly released live Actor may be reused; a stale Mass handle is not a registration.
		EnemyEntity = FMassEntityHandle();
	}
	BodyCollider->IgnoreActorWhenMoving(InPlanet->GetGameplaySurfaceActor(), true);
	FJTSPlanetSurfaceFrame Frame;
	if (InPlanet->GetSurfaceFrameAt(GroundLocation, GetActorForwardVector(), Frame))
	{
		SetActorLocationAndRotation(Frame.Location + Frame.Up * Behavior.HoverHeight,
			Frame.Transform.GetRotation(), false);
	}
	if (UJTSPlanetEnemySubsystem* Enemies = GetWorld()->GetSubsystem<UJTSPlanetEnemySubsystem>())
	{
		EnemyEntity = Enemies->RegisterEnemy(this, InPlanet, HomeLocation, GroundLocation, Behavior);
	}
	return EnemyEntity.IsValid();
}

void AJTSMoonCubeEnemy::OnSettlementAttackStarted_Implementation()
{
	if (HasAuthority()) MulticastAttackPulse();
}

void AJTSMoonCubeEnemy::OnSettlementForceImpact_Implementation(FVector Direction, float Speed)
{
	if (HasAuthority()) MulticastForceImpact(Direction, Speed);
}

void AJTSMoonCubeEnemy::ConfigurePresentation(UStaticMesh* InBodyMesh, UStaticMesh* InWeakPointMesh,
	UMaterialInterface* InBodyMaterial, UMaterialInterface* InWeakPointMaterial)
{
	if (!HasAuthority()) return;
	BodyMeshAsset = InBodyMesh;
	WeakPointMeshAsset = InWeakPointMesh;
	BodyMaterial = InBodyMaterial;
	WeakPointMaterial = InWeakPointMaterial;
	OnRep_PresentationAssets();
}

void AJTSMoonCubeEnemy::OnRep_PresentationAssets()
{
	if (IsValid(BodyMeshAsset)) BodyMesh->SetStaticMesh(BodyMeshAsset);
	if (IsValid(WeakPointMeshAsset)) WeakPointMesh->SetStaticMesh(WeakPointMeshAsset);
	if (IsValid(BodyMaterial)) BodyMesh->SetMaterial(0, BodyMaterial);
	if (IsValid(WeakPointMaterial)) WeakPointMesh->SetMaterial(0, WeakPointMaterial);
}

void AJTSMoonCubeEnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AJTSMoonCubeEnemy, BodyMeshAsset);
	DOREPLIFETIME(AJTSMoonCubeEnemy, WeakPointMeshAsset);
	DOREPLIFETIME(AJTSMoonCubeEnemy, BodyMaterial);
	DOREPLIFETIME(AJTSMoonCubeEnemy, WeakPointMaterial);
}

float AJTSMoonCubeEnemy::GetCriticalHitMultiplier(const FHitResult& Hit) const
{
	return Hit.GetComponent() == WeakPointCollider && IsValid(HealthComponent) && !HealthComponent->IsDead()
		? FMath::Max(1.0f, CriticalMultiplier) : 1.0f;
}

float AJTSMoonCubeEnemy::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority()) return 0.0f;
	DamageAttacker = EventInstigator != nullptr ? Cast<AJTSCharacter>(EventInstigator->GetPawn()) : nullptr;
	if (!DamageAttacker.IsValid()) DamageAttacker = Cast<AJTSCharacter>(DamageCauser);
	if (!DamageAttacker.IsValid() && IsValid(DamageCauser))
	{
		DamageAttacker = Cast<AJTSCharacter>(DamageCauser->GetInstigator());
		if (!DamageAttacker.IsValid()) DamageAttacker = Cast<AJTSCharacter>(DamageCauser->GetOwner());
	}
	bCurrentDamageCritical = DamageEvent.DamageTypeClass != nullptr
		&& DamageEvent.DamageTypeClass->IsChildOf(UJTSCriticalDamageType::StaticClass());
	CurrentDamageHitLocation = GetActorLocation() + GetActorUpVector() * 35.0f;
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		CurrentDamageHitLocation = static_cast<const FPointDamageEvent&>(DamageEvent).HitInfo.ImpactPoint;
	}
	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	bCurrentDamageCritical = false;
	DamageAttacker.Reset();
	return Applied;
}

void AJTSMoonCubeEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority()) return;
	FVector Location;
	FQuat Rotation;
	if (MotionBuffer.Sample(GetWorld()->GetTimeSeconds(), MovementInterpolationDelay, Location, Rotation))
	{
		const FVector Previous = GetActorLocation();
		// Body, visual mesh and critical-hit collider share the same continuous client transform.
		SetActorLocationAndRotation(Location, Rotation, false);
		BodyCollider->ComponentVelocity = DeltaSeconds > SMALL_NUMBER ? (Location - Previous) / DeltaSeconds : FVector::ZeroVector;
	}
}

void AJTSMoonCubeEnemy::OnRep_ReplicatedMovement()
{
	if (HasAuthority() || !GetWorld() || !IsReplicatingMovement())
	{
		Super::OnRep_ReplicatedMovement();
		return;
	}
	const FRepMovement& Snapshot = GetReplicatedMovement();
	MotionBuffer.Push(GetWorld()->GetTimeSeconds(), FRepMovement::RebaseOntoLocalOrigin(Snapshot.Location, this),
		Snapshot.Rotation.Quaternion(), Snapshot.LinearVelocity);
}

void AJTSMoonCubeEnemy::HandleDamaged(float CurrentHealth, float InMaxHealth, float Damage, AActor* DamageCauser)
{
	if (!HasAuthority()) return;
	MulticastDamageFeedback(CurrentHealth, InMaxHealth, Damage, CurrentDamageHitLocation, bCurrentDamageCritical);
	if (EnemyEntity.IsValid() && DamageAttacker.IsValid())
	{
		if (UJTSPlanetEnemySubsystem* Enemies = GetWorld()->GetSubsystem<UJTSPlanetEnemySubsystem>())
		{
			Enemies->NotifyDamaged(EnemyEntity, DamageAttacker.Get());
		}
	}
}

void AJTSMoonCubeEnemy::HandleDeath(AController* InstigatorController, AActor* DamageCauser)
{
	(void)InstigatorController;
	(void)DamageCauser;
	if (!HasAuthority()) return;
	if (EnemyEntity.IsValid())
	{
		if (UJTSPlanetEnemySubsystem* Enemies = GetWorld()->GetSubsystem<UJTSPlanetEnemySubsystem>())
		{
			Enemies->UnregisterEnemy(EnemyEntity);
		}
		EnemyEntity = FMassEntityHandle();
	}
	BodyCollider->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeakPointCollider->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetLifeSpan(0.8f);
}

void AJTSMoonCubeEnemy::ShowHealthBar(float CurrentHealth, float InMaxHealth)
{
	HealthBar->InitWidget();
	if (UJTSHealthBarWidget* Widget = Cast<UJTSHealthBarWidget>(HealthBar->GetUserWidgetObject()))
	{
		Widget->SetHealth(CurrentHealth, InMaxHealth);
	}
	HealthBar->SetVisibility(true);
	HealthBar->SetHiddenInGame(false);
	GetWorldTimerManager().ClearTimer(HealthBarTimer);
	GetWorldTimerManager().SetTimer(HealthBarTimer, this, &AJTSMoonCubeEnemy::HideHealthBar, 2.4f, false);
}

void AJTSMoonCubeEnemy::HideHealthBar()
{
	HealthBar->SetVisibility(false);
	HealthBar->SetHiddenInGame(true);
}

void AJTSMoonCubeEnemy::ResetVisualPulse()
{
	BodyMesh->SetRelativeRotation(FRotator::ZeroRotator);
	BodyMesh->SetRelativeScale3D(FVector(0.9f));
	WeakPointMesh->SetRelativeScale3D(FVector(0.34f, 0.34f, 0.25f));
}

void AJTSMoonCubeEnemy::MulticastForceImpact_Implementation(FVector_NetQuantizeNormal Direction, float Speed)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	const FVector Local = GetActorQuat().UnrotateVector(Direction);
	const float Strength = FMath::Clamp(Speed / 900.0f, 0.15f, 1.0f);
	BodyMesh->SetRelativeRotation(FRotator(Local.X * -18 * Strength, 0, Local.Y * 18 * Strength));
	BodyMesh->SetRelativeScale3D(FVector(0.9f + Strength * 0.13f, 0.9f + Strength * 0.13f, 0.9f - Strength * 0.2f));
	GetWorldTimerManager().ClearTimer(VisualPulseTimer);
	GetWorldTimerManager().SetTimer(VisualPulseTimer, this, &AJTSMoonCubeEnemy::ResetVisualPulse, 0.16f, false);
}

void AJTSMoonCubeEnemy::MulticastDamageFeedback_Implementation(float CurrentHealth, float InMaxHealth,
	float Damage, FVector_NetQuantize HitLocation, bool bCritical)
{
	if (GetNetMode() == NM_DedicatedServer || GetWorld() == nullptr) return;
	ShowHealthBar(CurrentHealth, InMaxHealth);
	BodyMesh->SetRelativeScale3D(bCritical ? FVector(1.02f, 1.02f, 0.76f) : FVector(0.96f, 0.96f, 0.83f));
	WeakPointMesh->SetRelativeScale3D(bCritical ? FVector(0.46f, 0.46f, 0.32f) : FVector(0.38f, 0.38f, 0.28f));
	GetWorldTimerManager().ClearTimer(VisualPulseTimer);
	GetWorldTimerManager().SetTimer(VisualPulseTimer, this, &AJTSMoonCubeEnemy::ResetVisualPulse, 0.13f, false);
	if (AJTSFloatingDamageActor* Popup = ActiveDamagePopup.Get())
	{
		Popup->AddDamage(Damage, bCritical);
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AJTSFloatingDamageActor* Popup = GetWorld()->SpawnActor<AJTSFloatingDamageActor>(
		AJTSFloatingDamageActor::StaticClass(), FTransform(GetActorRotation(), FVector(HitLocation) + GetActorUpVector() * 25.0f), Params))
	{
		Popup->Initialize(Damage, bCritical, GetActorUpVector());
		ActiveDamagePopup = Popup;
	}
}

void AJTSMoonCubeEnemy::MulticastAttackPulse_Implementation()
{
	if (GetNetMode() == NM_DedicatedServer) return;
	BodyMesh->SetRelativeScale3D(FVector(0.82f, 0.82f, 1.08f));
	WeakPointMesh->SetRelativeScale3D(FVector(0.40f, 0.40f, 0.30f));
	GetWorldTimerManager().ClearTimer(VisualPulseTimer);
	GetWorldTimerManager().SetTimer(VisualPulseTimer, this, &AJTSMoonCubeEnemy::ResetVisualPulse, 0.24f, false);
}
