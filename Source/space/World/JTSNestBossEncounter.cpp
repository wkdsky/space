#include "space/World/JTSNestBossEncounter.h"
#include "space/Components/JTSFacilityConditionComponent.h"
#include "space/World/JTSInteriorZone.h"
#include "Components/BoxComponent.h"
#include "space/Player/JTSCharacter.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "TimerManager.h"
#include "space/Characters/JTSPlanetBoss.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSPlanetEcologyHabitat.h"
#include "Engine/GameInstance.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSBossPhaseComponent.h"
#include "space/Items/JTSExpeditionKeyPickup.h"
#include "space/Systems/JTSExpeditionSubsystem.h"

AJTSNestBossEncounter::AJTSNestBossEncounter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Conditions=CreateDefaultSubobject<UJTSFacilityConditionComponent>(TEXT("Conditions"));
	Ridges = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SurfaceRidges"));
	Ridges->SetupAttachment(RootComponent);
	Ridges->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ridges->SetGenerateOverlapEvents(false);
	Ridges->SetCanEverAffectNavigation(false);
	Ridges->SetCastShadow(false);
}
void AJTSNestBossEncounter::BeginPlay()
{
	Super::BeginPlay();
	RebuildRidges();
	RewardOrigin = GetActorLocation();
	if (HasAuthority() && GetGameInstance())
		if (const auto* Expedition = GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>())
			if (!EncounterId.IsNone() && Expedition->GetSnapshot().DefeatedEncounterIds.Contains(EncounterId))
			{
				bTriggered = true;
				GetWorldTimerManager().SetTimer(RewardTimer, this, &ThisClass::SpawnReward, .5f);
				return;
			}
	if (HasAuthority()) GetWorldTimerManager().SetTimer(CheckTimer, this, &ThisClass::CheckNests, .25f, true, 1);
}
bool AJTSNestBossEncounter::AreAllNestsDestroyed() const
{
	if (Nests.IsEmpty()) return false;
	int32 Count=0;
	for (const AJTSPlanetEcologyHabitat* Nest : Nests) if (IsValid(Nest) && Nest->IsEntranceBlocked()) ++Count;
	return Count >= (MinimumDestroyedNests>0?MinimumDestroyedNests:Nests.Num());
}
bool AJTSNestBossEncounter::HasLivingParticipant() const
{
    for (TActorIterator<AJTSCharacter> It(GetWorld());It;++It)
        if (!It->IsBoarded() && !It->GetHealthComponent()->IsDead()
            && (TriggerRoom?TriggerRoom->Contains(It->GetActorLocation()):FVector::DistSquared(It->GetActorLocation(),GetActorLocation())<FMath::Square(6500.f))) return true;
    return false;
}
void AJTSNestBossEncounter::OnRep_ArenaClosed()
{
    for (const auto& D:ArenaDoors) if (IsValid(D)) { D->SetActorHiddenInGame(!bArenaClosed); D->SetActorEnableCollision(bArenaClosed); }
}
void AJTSNestBossEncounter::CheckNests()
{
	if (!HasAuthority()) return;
    if (bRoomTrigger && bTriggered && IsValid(ActiveBoss) && !ActiveBoss->Health->IsDead() && !HasLivingParticipant())
    {
        ActiveBoss->Destroy(); ActiveBoss=nullptr; bTriggered=false; bArenaClosed=false; OnRep_ArenaClosed(); ForceNetUpdate(); return;
    }
	if (bTriggered || !Planet || !BossClass || !Conditions->IsSatisfied()
        || (bRoomTrigger?(!TriggerRoom || !HasLivingParticipant()):!AreAllNestsDestroyed())) return;
	bTriggered = true;
    if (!bRoomTrigger) GetWorldTimerManager().ClearTimer(CheckTimer);
    bArenaClosed=bRoomTrigger; OnRep_ArenaClosed(); ForceNetUpdate();
	GetWorldTimerManager().SetTimer(WakeTimer, this, &ThisClass::WakeBoss, FMath::Max(.01f, WakeDelay));
}
void AJTSNestBossEncounter::WakeBoss()
{
	if (!HasAuthority() || IsValid(ActiveBoss) || !Planet || !BossClass) return;
	FJTSPlanetSurfaceHit Surface;
	if (!AJTSInteriorZone::ProjectGameplayGround(Planet,GetActorLocation(), Surface)) { bTriggered=false; bArenaClosed=false; OnRep_ArenaClosed(); return; }
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.OverrideLevel = GetLevel();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform Pose(Planet->MakeSurfaceAlignedRotation(Surface.ImpactPoint, GetActorForwardVector()), Surface.ImpactPoint);
	ActiveBoss = GetWorld()->SpawnActor<AJTSPlanetBoss>(BossClass, Pose, Params);
	if (ActiveBoss)
	{
		ActiveBoss->InitializeOnPlanet(Planet);
        if (auto* Phase=ActiveBoss->FindComponentByClass<UJTSBossPhaseComponent>())
            if (const auto* E=GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>())
                for (const auto& M:RemainingNestModifiers) if (!M.EntranceId.IsNone() && !E->IsEntranceBlocked(M.EntranceId))
                { Phase->EncounterArmor=FMath::Min(Phase->EncounterArmor,M.ArmorMultiplier); if (M.SummonClass) { Phase->BonusSummonClass=M.SummonClass; Phase->BonusSummonCount=M.SummonCount; } }
		ActiveBoss->Health->OnDeath.AddDynamic(this, &ThisClass::OnBossDefeated);
	}
	ForceNetUpdate();
}
void AJTSNestBossEncounter::OnBossDefeated(AController*, AActor*)
{
	if (!HasAuthority()) return;
    bArenaClosed=false; OnRep_ArenaClosed(); ForceNetUpdate();
	RewardOrigin = IsValid(ActiveBoss) ? ActiveBoss->GetActorLocation() : GetActorLocation();
	if (GetGameInstance())
		if (auto* Expedition = GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>()) Expedition->MarkEncounterDefeated(EncounterId);
	GetWorldTimerManager().SetTimer(RewardTimer, this, &ThisClass::SpawnReward, 1.f);
}
void AJTSNestBossEncounter::SpawnReward()
{
	if (!HasAuthority() || !Planet || !RewardClass || IsValid(ActiveReward)) return;
	const auto* Config = RewardClass.GetDefaultObject();
	if (!Config || Config->ImportantItemId.IsNone()) return;
	if (GetGameInstance())
		if (const auto* Expedition = GetGameInstance()->GetSubsystem<UJTSExpeditionSubsystem>())
			if (Expedition->GetSnapshot().ImportantItemIds.Contains(Config->ImportantItemId)) return;
	FJTSPlanetSurfaceHit Hit;
	const FVector Heading = IsValid(ActiveBoss) ? ActiveBoss->GetActorForwardVector() : GetActorForwardVector();
	FVector RewardPoint=RewardOrigin+Heading*RewardForwardOffset;
	if (TriggerRoom)
	{
		FVector Local=TriggerRoom->GetActorTransform().InverseTransformPosition(RewardPoint);
		const FVector Limit=TriggerRoom->Bounds->GetUnscaledBoxExtent()-FVector(130,130,40);
		Local.X=FMath::Clamp(Local.X,-Limit.X,Limit.X); Local.Y=FMath::Clamp(Local.Y,-Limit.Y,Limit.Y);
		Local.Z=FMath::Clamp(Local.Z,-Limit.Z,Limit.Z);
		RewardPoint=TriggerRoom->GetActorTransform().TransformPosition(Local);
		if (!AJTSInteriorZone::ProjectFloor(Planet,RewardPoint,Hit)) return;
	}
	else if (!AJTSInteriorZone::ProjectGameplayGround(Planet,RewardPoint,Hit)) return;
	FActorSpawnParameters Params; Params.Owner = this; Params.OverrideLevel = GetLevel();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ActiveReward = GetWorld()->SpawnActor<AJTSExpeditionKeyPickup>(RewardClass,
		Hit.ImpactPoint + Hit.ImpactNormal * 18, Planet->MakeSurfaceAlignedRotation(Hit.ImpactPoint, Heading), Params);
}
void AJTSNestBossEncounter::RebuildRidges()
{
	Ridges->ClearAllMeshSections();
	if (!Planet || Nests.IsEmpty()) return;
	const FVector Center = GetActorLocation();
	const FVector Up = Planet->GetRadialUpVector(Center);
	const FTransform Inverse = GetActorTransform().Inverse();
	int32 Section = 0;
	for (const AJTSPlanetEcologyHabitat* Nest : Nests)
	{
		if (!IsValid(Nest)) continue;
		const FVector Start = Nest->GetActorLocation();
		const FVector Direction = FVector::VectorPlaneProject(Start - Center, Up).GetSafeNormal();
		const FVector Side = FVector::CrossProduct(Up, Direction).GetSafeNormal();
		FJTSPlanetSurfaceHit End;
		if (!Planet->ProjectPointToSurface(Center + Direction * EndRingRadius, End)) continue;
		const int32 Steps = FMath::Clamp(FMath::CeilToInt(FVector::Dist(Start, End.ImpactPoint) / 45.f), 16, 256);
		TArray<FVector> Vertices, Normals;
		TArray<FVector2D> UV;
		TArray<FLinearColor> Colors;
		TArray<int32> Triangles;
		bool bSupported = true;
		for (int32 Row = 0; Row <= Steps && bSupported; ++Row)
		{
			const float T = float(Row) / Steps;
			const float Fade = FMath::Sin(PI * FMath::Clamp(T * 7, 0.f, .5f)) * FMath::Sin(PI * FMath::Clamp((1 - T) * 9, 0.f, .5f));
			const float Width = RidgeHalfWidth * (.22f + .78f * Fade) * (.8f + .16f * FMath::Sin(T * 42 + Section * 2));
			const FVector Mid = FMath::Lerp(Start, End.ImpactPoint, T) + Side * FMath::Sin(T * PI) * FMath::Sin(T * 13 + Section) * 28;
			for (int32 Column = 0; Column < 9; ++Column)
			{
				const float Across = (Column - 4) / 4.f;
				FJTSPlanetSurfaceHit Hit;
				if (!Planet->ProjectPointToSurface(Mid + Side * Across * Width, Hit)) { bSupported = false; break; }
				// Twin lifted lips flank the dark central split; all relief stays below walking collision.
				const float Lip = FMath::Pow(FMath::Max(0.f, FMath::Sin(PI * FMath::Abs(Across))), .7f);
				const float Ridge = Lip * RidgeHeight * Fade * (.9f + .1f * FMath::Sin(T * 139 + Section));
				const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
				Vertices.Add(Inverse.TransformPosition(Hit.ImpactPoint + Normal * (.65f + Ridge)));
				Normals.Add(Inverse.TransformVectorNoScale(Normal));
				UV.Add(FVector2D(T * FVector::Dist(Start, End.ImpactPoint) / 180, Column / 8.f));
				const float Shade = Column == 4 ? .38f : .70f + .22f * FMath::Abs(Across);
				Colors.Add(FLinearColor(Shade, Shade, Shade, 1));
				if (Row < Steps && Column < 8)
				{
					const int32 I = Row * 9 + Column;
					Triangles.Append({I, I + 9, I + 1, I + 1, I + 9, I + 10});
				}
			}
		}
		if (!bSupported) continue;
		Ridges->CreateMeshSection_LinearColor(Section, Vertices, Triangles, Normals, UV, Colors, TArray<FProcMeshTangent>(), false);
		Ridges->SetMaterial(Section++, RidgeMaterial);
	}
}
void AJTSNestBossEncounter::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(CheckTimer);
	GetWorldTimerManager().ClearTimer(WakeTimer);
	GetWorldTimerManager().ClearTimer(RewardTimer);
	Super::EndPlay(Reason);
}
void AJTSNestBossEncounter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AJTSNestBossEncounter, ActiveBoss);
    DOREPLIFETIME(AJTSNestBossEncounter,bArenaClosed);
}
