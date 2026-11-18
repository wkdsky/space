#include "space/Ships/JTSStellarExchangeDock.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Components/JTSStellarExchangeComponent.h"
#include "space/UI/JTSStellarExchangeWidget.h"
#include "space/Player/JTSPlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "space/Items/JTSWorldPickupActor.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSSpaceWorldManager.h"

AJTSStellarExchangeDock::AJTSStellarExchangeDock()
{
	Service = EJTSShipDockService::StellarExchange;
	bDeployable = false;
	PrizeEjectAnchor=CreateDefaultSubobject<USceneComponent>(TEXT("PrizeEjectAnchor")); PrizeEjectAnchor->SetupAttachment(DockRoot);
	LeverPivot = CreateDefaultSubobject<USceneComponent>(TEXT("LeverPivot")); LeverPivot->SetupAttachment(DockRoot);
	Lever = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lever")); Lever->SetupAttachment(LeverPivot);
	Lever->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LeverTarget = CreateDefaultSubobject<UBoxComponent>(TEXT("LeverTarget")); LeverTarget->SetupAttachment(LeverPivot);
	LeverTarget->InitBoxExtent(FVector(13, 13, 25));
	LeverTarget->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	LeverTarget->SetCollisionResponseToAllChannels(ECR_Ignore);
	LeverTarget->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Display = CreateDefaultSubobject<UWidgetComponent>(TEXT("Display")); Display->SetupAttachment(ScreenAnchor);
	Display->SetWidgetSpace(EWidgetSpace::World); Display->SetDrawSize(FVector2D(930, 222));
	Display->SetWidgetClass(UJTSStellarExchangeWidget::StaticClass()); Display->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Display->SetTwoSided(false); Display->SetTickWhenOffscreen(false);
}
void AJTSStellarExchangeDock::BeginPlay()
{
	Super::BeginPlay();
	Display->InitWidget();
	if (auto* W = Cast<UJTSStellarExchangeWidget>(Display->GetUserWidgetObject())) W->SetShip(GetSpacecraft(), false);
	// Only this one machine animates continuously; the other docks keep their dormant ticks.
	SetActorTickEnabled(true);
}
void AJTSStellarExchangeDock::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	auto* Ship = GetSpacecraft(); auto* Exchange = Ship ? Ship->GetStellarExchange() : nullptr;
	float Pull = 0;
	if (Exchange && GetNetMode()!=NM_DedicatedServer)
	{
		if (Exchange->IsRolling() && HeardSequence!=Exchange->GetState().Sequence)
		{
			HeardSequence=Exchange->GetState().Sequence;
			if (PullSound && Exchange->GetRollElapsed()<.3f) UGameplayStatics::PlaySoundAtLocation(this,PullSound,GetActorLocation(),.65f);
		}
		if (bWasRolling && !Exchange->IsRolling() && LockSound) UGameplayStatics::PlaySoundAtLocation(this,LockSound,GetActorLocation(),.65f);
		bWasRolling=Exchange->IsRolling();
	}
	if (Exchange && Exchange->IsRolling())
	{
		const float T = Exchange->GetRollElapsed();
		Pull = T < .32f ? FMath::SmoothStep(0.f, .32f, T) : 1.f - FMath::SmoothStep(.60f, 1.15f, T);
	}
	LeverPivot->SetRelativeRotation(FRotator(0, 0, PullAngle * Pull));
	if (auto* W = Cast<UJTSStellarExchangeWidget>(Display->GetUserWidgetObject())) W->SetShip(Ship, false);
	SetActorTickEnabled(true);
}
bool AJTSStellarExchangeDock::EjectPrize(const FJTSItemInstance& Item, APawn* Recipient)
{
	if (!HasAuthority()) return false;
	const FVector Origin=PrizeEjectAnchor->GetComponentLocation();
	FVector Gravity=FVector(0,0,GetWorld()->GetGravityZ());
	AJTSPlanetAnchor* Planet=nullptr;
	if (const auto* Manager=AJTSSpaceWorldManager::FindSpaceWorldManager(this)) Planet=Manager->GetCurrentPlanet();
	if (Planet) Gravity=Planet->GetGravityDirection(Origin)*Planet->GetGravityStrength();
	FVector Up=-Gravity.GetSafeNormal(); if (Up.IsNearlyZero()) Up=GetActorUpVector();
	const FVector Forward=FVector::VectorPlaneProject(PrizeEjectAnchor->GetForwardVector(),Up).GetSafeNormal();
	const FVector Target=Origin+Forward*PrizeForwardSpeed*.7f;
	FVector Landing;
	if (Planet)
	{
		FJTSPlanetSurfaceHit Hit; if (!Planet->ProjectPointToSurface(Target,Hit)) return false; Landing=Hit.ImpactPoint;
	}
	else
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(ArcadeEjectGround),false,this);
		if (auto* Ship=GetSpacecraft()) { Q.AddIgnoredActor(Ship); TArray<AActor*> Parts; Ship->GetAttachedActors(Parts,true,true); Q.AddIgnoredActors(Parts); }
		if (Recipient) Q.AddIgnoredActor(Recipient);
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByObjectType(Hit,Target+Up*150,Target-Up*2000,FCollisionObjectQueryParams(ECC_WorldStatic),Q)) return false;
		Landing=Hit.ImpactPoint;
	}
	const FTransform EjectionPose(GetActorQuat(),Origin);
	auto* Pickup=GetWorld()->SpawnActorDeferred<AJTSWorldPickupActor>(AJTSWorldPickupActor::StaticClass(),EjectionPose,nullptr,Recipient,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Pickup) return false;
	Pickup->InitializeItemInstance(Item); Pickup->FinishSpawning(EjectionPose);
	if (Planet) Pickup->PlaceOnPlanetSurface(Planet,Landing,Forward);
	Pickup->SetActorLocation(Origin,false,nullptr,ETeleportType::TeleportPhysics);
	Pickup->StartDropMotion(Up*PrizeUpwardSpeed+Forward*PrizeForwardSpeed,Gravity,Landing,this,Recipient);
	Pickup->ForceNetUpdate(); return true;
}

bool AJTSStellarExchangeDock::IsLookingAtLever(const APawn* Pawn) const
{
	if (!Pawn) return false;
	const FVector Eye = Pawn->GetPawnViewLocation();
	const FVector Forward = Pawn->GetViewRotation().Vector();
	return FVector::DotProduct((LeverTarget->GetComponentLocation() - Eye).GetSafeNormal(), Forward)
		> FVector::DotProduct((GetScreenWorldLocation() - Eye).GetSafeNormal(), Forward);
}
FVector AJTSStellarExchangeDock::GetInteractionWorldLocation(const FVector& Ref) const
{
	return FVector::DistSquared(Ref, LeverTarget->GetComponentLocation()) < FVector::DistSquared(Ref, GetScreenWorldLocation())
		? LeverTarget->GetComponentLocation() : GetScreenWorldLocation();
}
FText AJTSStellarExchangeDock::GetInteractionPrompt_Implementation(APawn* Pawn) const
{
	if (!CanUseDock(Pawn)) return FText::GetEmpty();
	if (IsLookingAtLever(Pawn))
	{
		const auto* Ship = GetSpacecraft(); const auto* E = Ship ? Ship->GetStellarExchange() : nullptr;
		return FText::FromString(E && E->IsRolling() ? TEXT("信号滚动中…") : TEXT("[E] 拉动阿尔法快乐拉杆"));
	}
	return FText::FromString(TEXT("[E] 阿尔法快乐拉杆（活动专用）"));
}
void AJTSStellarExchangeDock::Interact_Implementation(APawn* Pawn)
{
	if (!HasAuthority() || !CanUseDock(Pawn)) return;
	if (IsLookingAtLever(Pawn))
	{
		if (auto* S = GetSpacecraft())
		{
			const auto Result = S->GetStellarExchange()->TryPull(Pawn);
			if (auto* PC = Cast<AJTSPlayerController>(Pawn->GetController())) PC->ClientReceiveStellarRollResult(Result, NAME_None, INDEX_NONE);
		}
	}
	else Super::Interact_Implementation(Pawn);
}
