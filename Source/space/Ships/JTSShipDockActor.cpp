#include "space/Ships/JTSShipDockActor.h"
#include "space/Components/JTSSpacecraftDurabilityComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "space/Components/JTSShipDockComponent.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/World/JTSSpaceWorldManager.h"
#include "space/Items/JTSPortableCredentialDock.h"

AJTSShipDockActor::AJTSShipDockActor()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	DockRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DockRoot"));
	SetRootComponent(DockRoot);
	Mount = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mount"));
	Mount->SetupAttachment(DockRoot);
	UpperBayDoor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("UpperBayDoor"));
	UpperBayDoor->SetupAttachment(DockRoot);
	LowerBayDoor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LowerBayDoor"));
	LowerBayDoor->SetupAttachment(DockRoot);
	LeftStrut = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftStrut"));
	LeftStrut->SetupAttachment(DockRoot);
	RightStrut = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightStrut"));
	RightStrut->SetupAttachment(DockRoot);
	Carriage = CreateDefaultSubobject<USceneComponent>(TEXT("Carriage"));
	Carriage->SetupAttachment(DockRoot);
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Carriage);
	ScreenHinge = CreateDefaultSubobject<USceneComponent>(TEXT("ScreenHinge"));
	ScreenHinge->SetupAttachment(Carriage);
	ScreenHinge->SetRelativeLocation(FVector(0, 30, 20));
	Screen = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Screen"));
	Screen->SetupAttachment(ScreenHinge);
	TrayHinge = CreateDefaultSubobject<USceneComponent>(TEXT("TrayHinge"));
	TrayHinge->SetupAttachment(Carriage);
	TrayHinge->SetRelativeLocation(FVector(0, 30, -20));
	Tray = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tray"));
	Tray->SetupAttachment(TrayHinge);
	ScreenAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("ScreenAnchor"));
	ScreenAnchor->SetupAttachment(ScreenHinge);
	ScreenAnchor->SetRelativeLocation(FVector(0, 5, 20));
	ScreenTarget = CreateDefaultSubobject<UBoxComponent>(TEXT("ScreenTarget"));
	ScreenTarget->SetupAttachment(ScreenAnchor);
	ScreenTarget->InitBoxExtent(FVector(48, 5, 28));
	ScreenTarget->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ScreenTarget->SetCollisionObjectType(ECC_WorldDynamic);
	ScreenTarget->SetCollisionResponseToAllChannels(ECR_Ignore);
	ScreenTarget->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ScreenTarget->SetCanEverAffectNavigation(false);
	for (UStaticMeshComponent* Part : {Mount.Get(), UpperBayDoor.Get(), LowerBayDoor.Get(), LeftStrut.Get(), RightStrut.Get(), Body.Get(), Screen.Get(), Tray.Get()})
	{
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
	}
	for (UStaticMeshComponent* Part : {Body.Get(), Screen.Get(), Tray.Get()})
	{
		Part->SetCollisionResponseToAllChannels(ECR_Ignore);
		Part->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		Part->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}
}

void AJTSShipDockActor::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && CredentialDockClass)
	{
		FActorSpawnParameters Params; Params.Owner=this;
		CredentialDock=GetWorld()->SpawnActor<AJTSPortableCredentialDock>(CredentialDockClass,GetActorTransform(),Params);
		if (CredentialDock)
		{
			CredentialDock->AttachToComponent(Carriage,FAttachmentTransformRules::KeepRelativeTransform);
			CredentialDock->SetActorRelativeTransform(CredentialDockTransform);
		}
	}
	// Render moving hinges every frame, independently of the manager's low-frequency state checks.
	SetActorTickInterval(0);
	if (GetOwner()) AddTickPrerequisiteActor(GetOwner());
	ApplyPose();
	SetActorTickEnabled(!IsDeployed() && !IsStowed());
}
AJTSSpacecraftActor* AJTSShipDockActor::GetSpacecraft() const { return Cast<AJTSSpacecraftActor>(GetOwner()); }
void AJTSShipDockActor::EndPlay(EEndPlayReason::Type Reason)
{
	if (HasAuthority() && IsValid(CredentialDock)) CredentialDock->Destroy();
	Super::EndPlay(Reason);
}
void AJTSShipDockActor::InitializeDock(AJTSSpacecraftActor* Ship) { SetOwner(Ship); }

double AJTSShipDockActor::ServerTime() const
{
	const AGameStateBase* State = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return State ? State->GetServerWorldTimeSeconds() : GetWorld() ? GetWorld()->GetTimeSeconds() : 0;
}

float AJTSShipDockActor::GetDeploymentAlpha() const
{
	if (!bDeployable) return 1.f;
	const float Target = Pose.bDeployed ? 1.f : 0.f;
	const float Duration = FMath::Max(0.05f, Pose.bDeployed ? DeploymentDuration : RetractionDuration);
	const float Distance = FMath::Abs(Target - Pose.StartAlpha);
	const float Elapsed = FMath::Max(0.0, ServerTime() - Pose.ServerStartTime);
	return FMath::Lerp(Pose.StartAlpha, Target, FMath::Clamp(Elapsed / (Duration * FMath::Max(Distance, 0.001f)), 0.f, 1.f));
}

bool AJTSShipDockActor::IsDeployed() const { return !bDeployable || (Pose.bDeployed && GetDeploymentAlpha() >= 1.f); }
bool AJTSShipDockActor::IsStowed() const { return !bDeployable || (!Pose.bDeployed && GetDeploymentAlpha() <= 0.f); }
void AJTSShipDockActor::SetDeployed(bool bDeploy)
{
	if (!HasAuthority() || !bDeployable || Pose.bDeployed == bDeploy) return;
	Pose.StartAlpha = GetDeploymentAlpha();
	Pose.ServerStartTime = ServerTime();
	Pose.bDeployed = bDeploy;
	OnRep_Pose();
	ForceNetUpdate();
}
void AJTSShipDockActor::OnRep_Pose() { ApplyPose(); SetActorTickEnabled(true); }
void AJTSShipDockActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ApplyPose();
	if (IsDeployed() || IsStowed()) SetActorTickEnabled(false);
}
void AJTSShipDockActor::ApplyPose()
{
	if (!bDeployable)
	{
		ScreenTarget->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		return;
	}
	const float Alpha = GetDeploymentAlpha();
	auto Phase = [Alpha](float Begin, float End)
	{
		const float T = FMath::Clamp((Alpha - Begin) / (End - Begin), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	};
	// Reversible mechanism: open flush doors, extract the closed folio, lower it, unfold its two leaves.
	// Retraction closes the leaves completely before either lift or inward travel can begin.
	const float Door = Phase(0.f, 0.18f);
	UpperBayDoor->SetRelativeLocation(FVector(0, -12.f * FMath::Min(Door * 4.f, 1.f), BayDoorTravel * Door));
	LowerBayDoor->SetRelativeLocation(FVector(0, -12.f * FMath::Min(Door * 4.f, 1.f), -BayDoorTravel * Door));
	const FVector Extracted = FMath::Lerp(StowedOffset, ClearanceOffset, Phase(0.10f, 0.50f));
	Carriage->SetRelativeLocation(FMath::Lerp(Extracted, DeployedOffset, Phase(0.46f, 0.70f)));
	for (int32 Index = 0; Index < 2; ++Index)
	{
		UStaticMeshComponent* Strut = Index == 0 ? LeftStrut.Get() : RightStrut.Get();
		const FVector Start((Index == 0 ? -1 : 1) * StrutSpacing, -65, 28);
		const FVector End = Carriage->GetRelativeLocation() + FVector(Start.X, -4, 25);
		const FVector Delta = End - Start;
		Strut->SetRelativeLocation((Start + End) * 0.5f);
		Strut->SetRelativeRotation(FRotationMatrix::MakeFromZ(Delta).Rotator());
		Strut->SetRelativeScale3D(FVector(1, 1, FMath::Max(0.01f, Delta.Size() / 100.f)));
	}
	ScreenHinge->SetRelativeLocation(FMath::Lerp(FoldedScreenLocation, FVector(0, 30, 20), Phase(0.70f, 1.f)));
	TrayHinge->SetRelativeLocation(FMath::Lerp(FoldedTrayLocation, FVector(0, 30, -20), Phase(0.70f, 1.f)));
	ScreenHinge->SetRelativeRotation(FRotator(0, 0, FMath::Lerp(180.f, -8.f, Phase(0.70f, 1.f))));
	TrayHinge->SetRelativeRotation(FRotator(0, 0, FMath::Lerp(-90.f, 0.f, Phase(0.70f, 1.f))));
	ScreenTarget->SetCollisionEnabled(IsDeployed() ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	// Only stationary, fully deployed equipment blocks walkers. Moving mechanisms never push the ship/player.
	for (UStaticMeshComponent* Part : {Body.Get(), Screen.Get(), Tray.Get()})
	{
		Part->SetCollisionEnabled(IsDeployed() ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
}

FVector AJTSShipDockActor::GetScreenWorldLocation() const { return ScreenAnchor->GetComponentLocation(); }
FVector AJTSShipDockActor::GetInteractionWorldLocation(const FVector& ReferenceLocation) const
{
	FVector Closest = GetScreenWorldLocation();
	if (!bDeployable) return Closest;
	double BestDistance = TNumericLimits<double>::Max();
	for (const UStaticMeshComponent* Part : {Body.Get(), Screen.Get(), Tray.Get()})
	{
		if (!Part->GetStaticMesh()) continue;
		FVector Point;
		if (Part->GetClosestPointOnCollision(ReferenceLocation, Point) < 0.f)
			Point = Part->Bounds.GetBox().GetClosestPointTo(ReferenceLocation);
		const double Distance = FVector::DistSquared(ReferenceLocation, Point);
		if (Distance < BestDistance) { BestDistance = Distance; Closest = Point; }
	}
	return Closest;
}
bool AJTSShipDockActor::CanUseDock(const APawn* Pawn) const
{
	return CanUseDockAt(Pawn, GetInteractionWorldLocation(Pawn ? Pawn->GetPawnViewLocation() : GetActorLocation()));
}
bool AJTSShipDockActor::CanUseDockAt(const APawn* Pawn, const FVector& InteractionLocation) const
{
    if (const auto* H=GetOwner()?GetOwner()->FindComponentByClass<UJTSSpacecraftDurabilityComponent>():nullptr; H && H->bDisabled) return false;
	const AJTSSpacecraftActor* Ship = GetSpacecraft();
	const UJTSShipDockComponent* Modules = Ship ? Ship->GetDockComponent() : nullptr;
	if (!IsValid(Pawn) || !Modules || !Modules->IsParked()
		|| !IsDeployed() || Ship->IsPlayerBoarded(Pawn)) return false;
	if (Service == EJTSShipDockService::ShipTerminal)
	{
		const auto* Manager = AJTSSpaceWorldManager::FindSpaceWorldManager(Ship);
		if (!Manager || !Manager->IsSurfaceGameplayReady()) return false;
	}
	const FVector ToPawn = Pawn->GetActorLocation() - InteractionLocation;
	if (ToPawn.SizeSquared() > FMath::Square(InteractionDistance)) return false;
	// Any exposed device surface works, including the front of a protruding tray and its sides.
	// Keep the arm's interior/back inaccessible; the fixed central console still uses its display face.
	const FVector FrontOrigin = bDeployable ? GetActorLocation() : GetScreenWorldLocation();
	const FVector Normal = bDeployable ? GetActorRightVector() : ScreenAnchor->GetRightVector();
	if (FVector::DotProduct(Pawn->GetActorLocation() - FrontOrigin, Normal) <= 0.f) return false;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(ShipDockUse), false, Pawn);
	Query.AddIgnoredActor(this);
	if (CredentialDock) Query.AddIgnoredActor(CredentialDock);
	// The flight envelope spans empty space between the arms. Physical hull meshes still occlude use.
	Query.AddIgnoredComponent(Ship->GetFlightCollisionComponent());
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, Pawn->GetPawnViewLocation(), InteractionLocation, ECC_Visibility, Query);
}
bool AJTSShipDockActor::CanInteract_Implementation(APawn* Pawn) const { return CanUseDock(Pawn); }
FText AJTSShipDockActor::GetInteractionPrompt_Implementation(APawn* Pawn) const
{
	return CanUseDock(Pawn) ? NSLOCTEXT("JTS","UseShipDevice","[E] 使用") : FText::GetEmpty();
}
void AJTSShipDockActor::Interact_Implementation(APawn* Pawn)
{
	if (!HasAuthority() || !CanUseDock(Pawn)) return;
	if (AJTSPlayerController* Player = Cast<AJTSPlayerController>(Pawn->GetController()))
	{
		if (Service == EJTSShipDockService::Custom) OnDockUsed(Pawn);
		else Player->ClientOpenShipDock(this);
	}
}
void AJTSShipDockActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AJTSShipDockActor, Pose);
	DOREPLIFETIME(AJTSShipDockActor, CredentialDock);
}
