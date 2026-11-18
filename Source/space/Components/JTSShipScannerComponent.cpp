#include "space/Components/JTSShipScannerComponent.h"
#include "space/Components/JTSSpacecraftDurabilityComponent.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Ships/JTSShipDockActor.h"
#include "space/Components/JTSShipDockComponent.h"
#include "space/Components/JTSDiscoveryComponent.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSStellarLoadoutComponent.h"
#include "space/Player/JTSPlayerState.h"
#include "space/Components/JTSShipCabinCouplingComponent.h"
#include "space/Items/JTSWorldPickupActor.h"
#include "space/Characters/JTSPlanetSurfaceCreature.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/Core/JTSGameState.h"
#include "space/Systems/JTSExpeditionSubsystem.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UJTSShipScannerComponent::UJTSShipScannerComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick=true;
	PrimaryComponentTick.bStartWithTickEnabled=false;
}
void UJTSShipScannerComponent::BeginPlay()
{
	Super::BeginPlay();
	if(GetOwner()->HasAuthority())
	{
		if(auto* D=UJTSDiscoveryComponent::Find(this)) D->SetCatalog(AnalysisCatalog);
		GetWorld()->GetTimerManager().SetTimer(ScanTimer,this,&ThisClass::AdvanceScan,.1f,true);
	}
	if(GetNetMode()!=NM_DedicatedServer)
	{
		Projection=NewObject<UProceduralMeshComponent>(GetOwner()); Projection->SetupAttachment(this);
		Projection->SetCollisionEnabled(ECollisionEnabled::NoCollision); Projection->SetCanEverAffectNavigation(false);
		Projection->SetCastShadow(false); Projection->RegisterComponent(); Projection->SetMaterial(0,ProjectionMaterial);
		OnRep_Zone();
	}
}
void UJTSShipScannerComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if(GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ScanTimer);
	if(Projection) Projection->DestroyComponent();
	Super::EndPlay(Reason);
}
double UJTSShipScannerComponent::ServerTime() const
{
	const auto* S=GetWorld()?GetWorld()->GetGameState():nullptr;
	return S?S->GetServerWorldTimeSeconds():GetWorld()?GetWorld()->GetTimeSeconds():0;
}
float UJTSShipScannerComponent::DurationForSpan(float Span) const
{ return FMath::Lerp(SmallSampleSeconds,LargeSampleSeconds,FMath::Clamp((Span-12.f)/FMath::Max(1.f,LargeSampleSpan-12.f),0.f,1.f)); }
float UJTSShipScannerComponent::GetScanElapsed() const
{ return FMath::Clamp(Scan.Elapsed+(Scan.bRunning?FMath::Max(0.0,ServerTime()-Scan.StartedAt):0.0),0.0,double(Scan.Duration)); }
float UJTSShipScannerComponent::GetScanProgress() const
{ return Scan.bComplete?1.f:Scan.Duration>0?GetScanElapsed()/Scan.Duration:0.f; }
bool UJTSShipScannerComponent::ContainsPoint(const FVector& Point) const
{
	if(!bZoneActive) return false;
	const FVector P=GroundFrame.InverseTransformPositionNoScale(Point);
	return P.Z>=-15 && P.Z<=ZoneHeight && FVector2D(P.X,P.Y).SizeSquared()<=FMath::Square(ZoneRadius);
}
bool UJTSShipScannerComponent::ResolveGround(FTransform& Out) const
{
    if (const auto* H=GetOwner()->FindComponentByClass<UJTSSpacecraftDurabilityComponent>(); H && H->bDisabled) return false;
	const auto* Ship=Cast<AJTSSpacecraftActor>(GetOwner());
	if(!Ship || !Ship->GetDockComponent()->IsParked() || !Ship->CabinCoupling->IsCabinDocked()) return false;
	FVector Desired=GetComponentLocation(),Forward=GetForwardVector(),Up=Ship->GetActorUpVector();
	if(auto* Terminal=Ship->GetDockComponent()->GetTerminal())
	{
		const FTransform Screen=Terminal->ScreenAnchor->GetComponentTransform();
		Desired=Screen.TransformPositionNoScale(OffsetFromScreen); Forward=Screen.GetUnitAxis(EAxis::Y);
	}
	FVector Ground;
	if(auto* Planet=Ship->GetGroundedPlanet())
	{
		FJTSPlanetSurfaceHit Hit;
		if(!Planet->ProjectPointToSurface(Desired,Hit)) return false;
		Ground=Hit.ImpactPoint; Up=Hit.ImpactNormal;
	}
	else
	{
		FCollisionQueryParams Query(SCENE_QUERY_STAT(ShipScanGround),false,Ship);
		TArray<AActor*> Children; Ship->GetAttachedActors(Children,true,true); Query.AddIgnoredActors(Children);
		FHitResult Hit;
		if(!GetWorld()->LineTraceSingleByObjectType(Hit,Desired+Up*300,Desired-Up*1200,FCollisionObjectQueryParams(ECC_WorldStatic),Query)) return false;
		Ground=Hit.ImpactPoint; Up=Hit.ImpactNormal;
	}
	Forward=FVector::VectorPlaneProject(Forward,Up).GetSafeNormal();
	if(Forward.IsNearlyZero()) Forward=FVector::VectorPlaneProject(Ship->GetActorRightVector(),Up).GetSafeNormal();
	Out=FTransform(FRotationMatrix::MakeFromXZ(Forward,Up).ToQuat(),Ground);
	return true;
}
void UJTSShipScannerComponent::ResetScan()
{
	if(Scan.Sample.IsEmpty()) return;
	Scan=FJTSGroundScanState(); LastCheckpoint=0; GetOwner()->ForceNetUpdate(); Persist();
}
void UJTSShipScannerComponent::AdvanceScan()
{
	if(!GetOwner()->HasAuthority()) return;
	FTransform Ground;
	const bool Active=ResolveGround(Ground);
	if(Active) GroundFrame=Ground;
	if(bZoneActive!=Active) { bZoneActive=Active; OnRep_Zone(); GetOwner()->ForceNetUpdate(); }
	if(!Active) { ResetScan(); return; }
	if(!PendingRestore.Item.IsEmpty())
	{
		FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* P=GetWorld()->SpawnActor<AJTSWorldPickupActor>(GroundFrame.GetLocation()+GroundFrame.GetUnitAxis(EAxis::Z)*20,GroundFrame.Rotator(),Params);
		if(P)
		{
			P->InitializeItemInstance(PendingRestore.Item);
			const auto* Ship=Cast<AJTSSpacecraftActor>(GetOwner());
			if(auto* Planet=Ship?Ship->GetGroundedPlanet():nullptr) P->PlaceOnPlanetSurface(Planet,GroundFrame.GetLocation(),GroundFrame.GetUnitAxis(EAxis::X));
			else P->AdjustToGround(GroundFrame.GetLocation());
			Scan.Target=P; Scan.Sample=P->GetItemInstance(); Scan.Duration=FMath::Max(.1f,PendingRestore.Duration);
			Scan.Elapsed=PendingRestore.bComplete?Scan.Duration:PendingRestore.Elapsed;
			Scan.bUnknown=!UJTSDiscoveryComponent::IsKnown(this,Scan.Sample);
			Scan.StartedAt=ServerTime(); Scan.bRunning=true;
			PendingRestore=FJTSStoredAnalysisSample(); Persist();
		}
	}
	if(Scan.bComplete)
	{
		if(ServerTime()-Scan.CompletedAt>=1.1) ResetScan();
		return;
	}
	if(!Scan.Sample.IsEmpty() && (!IsValid(Scan.Target) || !Scan.Target->IsAvailableForShipScan() || !ContainsPoint(Scan.Target->GetInteractionAnchorWorldLocation()) || Scan.Target->GetItemInstance().InstanceId!=Scan.Sample.InstanceId)) ResetScan();
	if(Scan.Sample.IsEmpty())
	{
		AJTSWorldPickupActor* Candidate=nullptr; double Best=TNumericLimits<double>::Max();
		for(TActorIterator<AJTSWorldPickupActor> It(GetWorld());It;++It)
		{
			if(!It->IsAvailableForShipScan() || !ContainsPoint(It->GetInteractionAnchorWorldLocation())) continue;
			const double Distance=FVector::DistSquared(It->GetActorLocation(),GroundFrame.GetLocation());
			if(Distance<Best) { Candidate=*It; Best=Distance; }
		}
		if(!Candidate) for(TActorIterator<AJTSPlanetSurfaceCreature> It(GetWorld());It;++It)
			if(ContainsPoint(It->GetActorLocation())) if(auto* Corpse=It->ReleaseCorpseForAnalysis(GroundFrame)) { Candidate=Corpse; break; }
		if(!Candidate) return;
		Scan.Target=Candidate; Scan.Sample=Candidate->GetItemInstance();
		Scan.Duration=UJTSDiscoveryComponent::IsKnown(this,Scan.Sample)?1.2f:DurationForSpan(Scan.Sample.SampleSpanCm);
		Scan.bUnknown=!UJTSDiscoveryComponent::IsKnown(this,Scan.Sample);
		Scan.bRunning=true; Scan.StartedAt=ServerTime()+.3; LastCheckpoint=0;
		GetOwner()->ForceNetUpdate(); Persist();
	}
	if(GetScanProgress()<1)
	{
		if(GetScanElapsed()-LastCheckpoint>=1) { LastCheckpoint=GetScanElapsed(); Persist(); }
		return;
	}
	// Identification succeeds even when equipment storage is full; the real object stays available.
	if(auto* D=UJTSDiscoveryComponent::Find(this)) D->Analyze(Scan.Sample); else return;
	if(!CanStore(Scan.Sample))
	{
		if(!Scan.bStorageBlocked) { Scan.bStorageBlocked=true; Scan.Elapsed=Scan.Duration; Scan.bRunning=false; GetOwner()->ForceNetUpdate(); Persist(); }
		return;
	}
	if(!Scan.Target->ReserveForShipScan()) { ResetScan(); return; }
	if(!Store(Scan.Sample)) { Scan.Target->CancelShipScanReservation(); return; }
	Scan.Target->CompleteShipCollection(CollectionMaterial);
	Scan.Elapsed=Scan.Duration; Scan.bRunning=false; Scan.bComplete=true; Scan.bStorageBlocked=false; Scan.CompletedAt=ServerTime();
	GetOwner()->ForceNetUpdate(); Persist();
}
bool UJTSShipScannerComponent::CanStore(const FJTSItemInstance& Item) const
{
	const auto* Ship=Cast<AJTSSpacecraftActor>(GetOwner()); EJTSResourceType Type;
	if(UJTSItemDefinitionLibrary::TryGetResourceType(Item.ItemId,Type))
		return Ship && Item.StackCount>0 && int64(Ship->GetResourceAmount(Type))+Item.StackCount<=MAX_int32;
	return Cargo.Num()<CargoCapacity || Cargo.ContainsByPredicate([&Item](const auto& I){return I.IsEmpty() || (Item.IsStellar() && I.CanStackWith(Item) && I.StackCount<=MAX_int32-Item.StackCount);});
}
bool UJTSShipScannerComponent::Store(const FJTSItemInstance& Item)
{
	auto* Ship=Cast<AJTSSpacecraftActor>(GetOwner()); EJTSResourceType Type;
	if(UJTSItemDefinitionLibrary::TryGetResourceType(Item.ItemId,Type)) return Ship && Ship->DepositResourceAmounts({{Type,Item.StackCount}});
	if(!CanStore(Item)) return false;
	if (Item.IsStellar()) for(auto& Existing:Cargo) if(Existing.CanStackWith(Item) && Existing.StackCount<=MAX_int32-Item.StackCount) { Existing.StackCount+=Item.StackCount; return true; }
	const int32 Empty=Cargo.IndexOfByPredicate([](const auto& I){return I.IsEmpty();});
	if(Empty==INDEX_NONE) Cargo.Add(Item); else Cargo[Empty]=Item;
	return true;
}
bool UJTSShipScannerComponent::PlaceCarriedSample(APawn* Pawn,int32 Slot,FGuid ExpectedId)
{
	auto* Ship=Cast<AJTSSpacecraftActor>(GetOwner());
	auto* I=Pawn?Pawn->FindComponentByClass<UJTSInventoryComponent>():nullptr;
	const auto* H=Pawn?Pawn->FindComponentByClass<UJTSHealthComponent>():nullptr;
	if(!Ship || !Ship->HasAuthority() || !I || (H && H->IsDead()) || !bZoneActive || !Ship->GetDockComponent()->IsParked() || !Ship->CabinCoupling->IsCabinDocked() || Ship->IsPlayerBoarded(Pawn)
		|| FVector::DistSquared(Pawn->GetActorLocation(),GroundFrame.GetLocation())>FMath::Square(400.f)) return false;
	const auto Item=I->GetItemAtSlot(Slot);
	if(Item.IsEmpty() || !ExpectedId.IsValid() || Item.InstanceId!=ExpectedId) return false;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(PlaceShipScanSample),false,Pawn);
	Query.AddIgnoredComponent(Ship->GetFlightCollisionComponent());
	FHitResult Hit;
	if(GetWorld()->LineTraceSingleByChannel(Hit,Pawn->GetPawnViewLocation(),GroundFrame.GetLocation()+GroundFrame.GetUnitAxis(EAxis::Z)*20,ECC_Visibility,Query)) return false;
	auto* P=GetWorld()->SpawnActorDeferred<AJTSWorldPickupActor>(AJTSWorldPickupActor::StaticClass(),GroundFrame,nullptr,Pawn,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if(!P) return false;
	P->InitializeItemInstance(Item); P->FinishSpawning(GroundFrame);
	if(auto* Planet=Ship->GetGroundedPlanet()) P->PlaceOnPlanetSurface(Planet,GroundFrame.GetLocation(),GroundFrame.GetUnitAxis(EAxis::X));
	else P->AdjustToGround(GroundFrame.GetLocation());
	FJTSItemInstance Extracted;
	if(!I->TryExtractItemAtSlot(Slot,ExpectedId,Extracted)) { P->Destroy(); return false; }
	return true;
}
bool UJTSShipScannerComponent::RetrieveCargo(APawn* Pawn,int32 Slot,FGuid ExpectedId)
{
	auto* Ship=Cast<AJTSSpacecraftActor>(GetOwner()); auto* I=Pawn?Pawn->FindComponentByClass<UJTSInventoryComponent>():nullptr;
	const auto* H=Pawn?Pawn->FindComponentByClass<UJTSHealthComponent>():nullptr;
	if(!Ship || !Ship->HasAuthority() || !I || (H && H->IsDead()) || !Cargo.IsValidIndex(Slot)
		|| !Ship->GetDockComponent()->CanUseService(EJTSShipDockService::ShipTerminal,Pawn)) return false;
	const auto Item=Cargo[Slot];
	if(Item.IsEmpty() || !ExpectedId.IsValid() || Item.InstanceId!=ExpectedId) return false;
	if(Item.IsStellar())
	{
		auto* PS=Pawn->GetPlayerState<AJTSPlayerState>(); if(!PS) return false;
		PS->GetStellarLoadout()->ConfigureLootTable(const_cast<UJTSStellarLootTable*>(Ship->GetStellarLootTable()));
		if(!PS->GetStellarLoadout()->TryStoreItem(Item)) return false;
	}
	else
	{
		if(!I->CanAddItemInstance(Item)) return false;
		int32 Left=0; if(!I->TryAddItem(Item,Left) || Left) return false;
	}
	Cargo[Slot].Clear(); GetOwner()->ForceNetUpdate(); Persist(); return true;
}
FJTSStoredAnalysisSample UJTSShipScannerComponent::ExportPendingSample() const
{
	if(!PendingRestore.Item.IsEmpty()) return PendingRestore;
	FJTSStoredAnalysisSample Saved;
	if(!Scan.bComplete && IsValid(Scan.Target) && Scan.Target->IsAvailableForShipScan() && Scan.Target->GetItemInstance().InstanceId==Scan.Sample.InstanceId)
	{ Saved.Item=Scan.Sample; Saved.Duration=Scan.Duration; Saved.Elapsed=GetScanElapsed(); }
	return Saved;
}
void UJTSShipScannerComponent::RestoreState(const TArray<FJTSItemInstance>& Items,const FJTSStoredAnalysisSample& Sample)
{
	if(!GetOwner()->HasAuthority()) return;
	Cargo=Items; PendingRestore=Sample; GetOwner()->ForceNetUpdate();
}
void UJTSShipScannerComponent::Persist()
{
	if(auto* GI=GetWorld()->GetGameInstance()) if(auto* E=GI->GetSubsystem<UJTSExpeditionSubsystem>())
	{
		E->GetMutableSnapshot().AnalysisSample=ExportPendingSample(); E->GetMutableSnapshot().ScannerCargo=Cargo;
		E->CaptureWorldState(GetWorld()->GetGameState<AJTSGameState>(),Cast<AJTSSpacecraftActor>(GetOwner())); E->RequestSave();
	}
}
void UJTSShipScannerComponent::OnRep_Zone()
{
	if(Projection) Projection->SetVisibility(bZoneActive);
	SetComponentTickEnabled(bZoneActive && GetNetMode()!=NM_DedicatedServer);
}
void UJTSShipScannerComponent::TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Function)
{
	Super::TickComponent(Dt,Type,Function); if(!bZoneActive) return;
	if(Projection) { Projection->SetWorldTransform(GroundFrame); BuildProjection(); }
}
void UJTSShipScannerComponent::BuildProjection()
{
	TArray<FVector> V; TArray<int32> T; TArray<FVector2D> UV; TArray<FLinearColor> Colors;
	const FLinearColor C=Scan.bStorageBlocked?FLinearColor(1,.38f,.08f):Scan.bComplete?FLinearColor(.4f,1,.7f):FLinearColor(.08f,.85f,1);
	auto Ring=[&](float Radius,float Width,float Progress,float Alpha,float Height)
	{
		for(int32 I=0;I<96 && float(I)/96<Progress;++I)
		{
			const float A=I*2*PI/96,B=FMath::Min(float(I+1)/96,Progress)*2*PI; const int32 N=V.Num();
			for(const FVector2D P:{FVector2D(FMath::Cos(A),FMath::Sin(A)),FVector2D(FMath::Cos(B),FMath::Sin(B))})
				for(float R:{Radius-Width,Radius}) { V.Add(FVector(P.X*R,P.Y*R,Height)); UV.Add(P*.5f+FVector2D(.5)); Colors.Add(C*Alpha); }
			T.Append({N,N+1,N+2,N+1,N+3,N+2});
		}
	};
	const float Progress=GetScanProgress();
	const float Time=float(ServerTime());
	const bool Scanning=Scan.bRunning && !Scan.bStorageBlocked;
	const float Envelope=Scanning ? FMath::Min(1.f,GetScanElapsed()/.35f) : 0.f;
	Ring(ZoneRadius,1.8f,1,Scanning?.8f:.22f,2.5f);
	auto Quad=[&](FVector A,FVector B,FVector D,FVector E,float Alpha)
	{
		const int32 N=V.Num();
		V.Append({A,B,D,E}); T.Append({N,N+1,N+2,N+2,N+1,N+3});
		UV.Append({FVector2D(0,0),FVector2D(1,0),FVector2D(0,1),FVector2D(1,1)});
		for(int32 I=0;I<4;++I) Colors.Add(FLinearColor(C.R,C.G,C.B,Alpha));
	};
	auto Ray=[&](FVector A,FVector B,float Width,float Alpha)
	{
		const FVector Side=FVector::CrossProduct(B-A,FVector::UpVector).GetSafeNormal(UE_SMALL_NUMBER,FVector::RightVector)*Width;
		Quad(A-Side,A+Side,B-Side,B+Side,Alpha);
	};
	if(Scanning)
	{
		// Sparse vertical streaks keep the sample readable through the column.
		for(int32 I=0;I<160;++I)
		{
			const float A=I*2.399963f;
			const float Radius=ZoneRadius*(I%5==0 ? FMath::Sqrt(FMath::Frac(I*.618034f)) : .98f);
			const float Phase=FMath::Frac(Time*(.38f+float(I%7)*.025f)+I*.618034f);
			const float Z=Phase*ColumnHeight;
			const FVector Base(Radius*FMath::Cos(A),Radius*FMath::Sin(A),Z+3);
			const float Fade=FMath::Sin(Phase*PI)*Envelope;
			Quad(Base-FVector(1,0,0),Base+FVector(1,0,0),Base+FVector(-1,0,7),Base+FVector(1,0,7),Fade*.55f);
			Quad(Base-FVector(0,1,0),Base+FVector(0,1,0),Base+FVector(0,-1,7),Base+FVector(0,1,7),Fade*.55f);
		}
		if(Scan.bUnknown)
		{
			const auto* Ship=Cast<AJTSSpacecraftActor>(GetOwner());
			const auto* Terminal=Ship ? Ship->GetDockComponent()->GetTerminal() : nullptr;
			if(Terminal)
			{
				const FVector Source=GroundFrame.InverseTransformPositionNoScale(Terminal->ScreenAnchor->GetComponentTransform().TransformPositionNoScale(CameraOffsetFromScreen));
				// One continuous far -> near -> far pass, synchronized to server scan time.
				const float CoverageRadius=ZoneRadius*FMath::Max(1.15f,ScanCoverageScale);
				const float X=FMath::Lerp(ZoneRadius*1.15f,-ZoneRadius*1.15f,Progress);
				const float HalfChord=FMath::Sqrt(FMath::Max(0.f,FMath::Square(CoverageRadius)-X*X));
				for(int32 I=0;I<=24;++I)
				{
					const FVector End(X,FMath::Lerp(-HalfChord,HalfChord,float(I)/24),4);
					Ray(Source+FVector(0,(I-12)*.45f,0),End,.38f,Envelope*.26f);
				}
				Ray(FVector(X,-HalfChord,4),FVector(X,HalfChord,4),1.5f,Envelope*.85f);
				Quad(Source,Source+FVector(0,.1f,0),FVector(X,-HalfChord,4),FVector(X,HalfChord,4),Envelope*.025f);
			}
		}
	}
	if(Scan.bComplete && IsValid(Scan.Target))
	{
		const float P=FMath::Clamp(float(ServerTime()-Scan.CompletedAt)/.9f,0.f,1.f);
		const FVector Center=GroundFrame.InverseTransformPositionNoScale(Scan.Target->GetActorLocation());
		for(int32 I=0;I<96;++I)
		{
			const float A=I*2.399963f;
			const float Radius=FMath::Max(12.f,Scan.Sample.SampleSpanCm*.4f)*FMath::Sqrt(FMath::Frac(I*.618034f));
			const FVector Point=Center+FVector(FMath::Cos(A)*Radius*(1-P),FMath::Sin(A)*Radius*(1-P),P*(75+I%43));
			Quad(Point-FVector(1.5f,0,0),Point+FVector(1.5f,0,0),Point+FVector(-1.5f,0,3),Point+FVector(1.5f,0,3),(1-P)*.8f);
		}
	}

	Projection->CreateMeshSection_LinearColor(0,V,T,{},UV,Colors,{},false);
}
void UJTSShipScannerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UJTSShipScannerComponent,bZoneActive); DOREPLIFETIME(UJTSShipScannerComponent,GroundFrame);
	DOREPLIFETIME(UJTSShipScannerComponent,Scan); DOREPLIFETIME(UJTSShipScannerComponent,Cargo);
}
