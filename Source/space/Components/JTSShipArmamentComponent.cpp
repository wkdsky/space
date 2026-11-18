#include "space/Components/JTSShipArmamentComponent.h"
#include "space/Ships/JTSShipWeaponModule.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Player/JTSPlayerState.h"
#include "space/Player/JTSCharacter.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSBossCombatComponent.h"
#include "space/Components/JTSStellarTargetComponent.h"
#include "space/Components/JTSSpacecraftDurabilityComponent.h"
#include "space/Interaction/JTSMeleeTarget.h"
#include "space/World/JTSPlanetSettlementEnemy.h"
#include "Engine/OverlapResult.h"
#include "Components/SceneComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

UJTSShipArmamentComponent::UJTSShipArmamentComponent(){SetIsReplicatedByDefault(true);PrimaryComponentTick.bCanEverTick=false;}
void UJTSShipArmamentComponent::BeginPlay()
{
	Super::BeginPlay();
	if(GetOwner()->HasAuthority())
	{
		Modules.SetNum(Hardpoints.Num());
		for(const auto& H:Hardpoints)ReplaceWeapon(H.Id,H.WeaponClass);
	}
	GetWorld()->GetTimerManager().SetTimer(UpdateTimer,this,&ThisClass::Update,.05f,true);
}
bool UJTSShipArmamentComponent::ReplaceWeapon(FName Id,TSubclassOf<AJTSShipWeaponModule> Class)
{
	if(!GetOwner()->HasAuthority())return false;
	const int32 I=Hardpoints.IndexOfByPredicate([Id](const auto& H){return H.Id==Id;});
	if(I==INDEX_NONE)return false;
	Modules.SetNum(Hardpoints.Num());
	AJTSShipWeaponModule* New=nullptr;
	if(Class)
	{
		FActorSpawnParameters P;P.Owner=GetOwner();P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		New=GetWorld()->SpawnActor<AJTSShipWeaponModule>(Class,Hardpoints[I].MountTransform*GetOwner()->GetActorTransform(),P);
		if(!New)return false;
		USceneComponent* Parent=GetOwner()->GetRootComponent();
		for(auto* C:TInlineComponentArray<USceneComponent*>(GetOwner()))
			if(C->GetFName()==Hardpoints[I].ParentComponentName) {Parent=C;break;}
		New->AttachToComponent(Parent,FAttachmentTransformRules::KeepWorldTransform);
	}
	if(IsValid(Modules[I]))Modules[I]->Destroy();Modules[I]=New;GetOwner()->ForceNetUpdate();return true;
}
bool UJTSShipArmamentComponent::HasDriver()const
{
	const auto* Ship=Cast<AJTSSpacecraftActor>(GetOwner());
	const auto* PC=Ship?Cast<APlayerController>(Ship->GetController()):nullptr;
	return PC && Ship->GetDriverPlayerState() && PC->GetPlayerState<AJTSPlayerState>()==Ship->GetDriverPlayerState();
}
void UJTSShipArmamentComponent::ServerSetWeaponIntent_Implementation(FVector_NetQuantizeNormal Dir,bool Fire)
{
	const auto* Ship=Cast<AJTSSpacecraftActor>(GetOwner());
	if(!HasDriver() || !Ship || Ship->IsLanded() || Dir.ContainsNaN() || !FMath::IsNearlyEqual(Dir.SizeSquared(),1.f,.03f))return;
	AimDirection=Dir.GetSafeNormal();bFiring=Fire;LastIntentTime=GetWorld()->GetTimeSeconds();
}
void UJTSShipArmamentComponent::Update()
{
	auto* Ship=Cast<AJTSSpacecraftActor>(GetOwner());if(!Ship)return;
	auto* PC=Cast<APlayerController>(Ship->GetController());
	if(!Ship->IsLanded() && Ship->IsLocallyControlled() && PC && PC->IsLocalController())
	{
		FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);
		const bool Fire=!PC->bShowMouseCursor && PC->IsInputKeyDown(EKeys::LeftMouseButton);
		ServerSetWeaponIntent(View.Vector(),Fire);
	}
	if(!Ship->HasAuthority())return;
	if (Ship->IsLanded())
	{
		bFiring = false;
		LastIntentTime = -1;
		UpdateAutomaticDefense();
		return;
	}
	AutomaticTargets.Reset();
	NextTargetRefreshTime = 0;
	const bool Fire=HasDriver() && !Ship->IsLanded() && bFiring && GetWorld()->GetTimeSeconds()-LastIntentTime<.3;
	for(int32 I=0;I<Modules.Num();++I)if(IsValid(Modules[I]) && Hardpoints.IsValidIndex(I))
	{
		Modules[I]->Aim(HasDriver()?AimDirection:Ship->GetActorForwardVector(),Hardpoints[I].bFullRotation);
		if(Fire)Modules[I]->FirePulse();
	}
}
void UJTSShipArmamentComponent::RefreshAutomaticTargets()
{
	AutomaticTargets.Reset();
	TArray<FOverlapResult> Hits;
	GetWorld()->OverlapMultiByObjectType(Hits, GetOwner()->GetActorLocation(), FQuat::Identity,
		FCollisionObjectQueryParams::AllDynamicObjects, FCollisionShape::MakeSphere(AutomaticTargetRange));
	for (const FOverlapResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		if (!IsValid(Target) || Target == GetOwner() || Target->IsA<AJTSCharacter>() || Target->IsHidden()) continue;
		const auto* Health = Target->FindComponentByClass<UJTSHealthComponent>();
		if (!Health || Health->IsDead() || Health->IsInvulnerable()) continue;
		if (!Target->FindComponentByClass<UJTSStellarTargetComponent>()
			&& !Target->Implements<UJTSPlanetSettlementEnemy>() && !Target->FindComponentByClass<UJTSBossCombatComponent>()) continue;
		AutomaticTargets.AddUnique(Target);
	}
	AutomaticTargets.Sort([this](const auto& A, const auto& B)
	{
		return FVector::DistSquared(A->GetActorLocation(), GetOwner()->GetActorLocation())
			< FVector::DistSquared(B->GetActorLocation(), GetOwner()->GetActorLocation());
	});
	NextTargetRefreshTime = GetWorld()->GetTimeSeconds() + .25;
}
void UJTSShipArmamentComponent::UpdateAutomaticDefense()
{
	const auto* Hull = GetOwner()->FindComponentByClass<UJTSSpacecraftDurabilityComponent>();
	if (Hull && Hull->bDisabled) return;
	if (GetWorld()->GetTimeSeconds() >= NextTargetRefreshTime) RefreshAutomaticTargets();
	for (int32 I = 0; I < Modules.Num(); ++I)
	{
		AJTSShipWeaponModule* Module = Modules[I];
		if (!IsValid(Module) || !Hardpoints.IsValidIndex(I)) continue;
		bool bHasTarget = false;
		for (const auto& Candidate : AutomaticTargets)
		{
			AActor* Target = Candidate.Get();
			const auto* Health = Target ? Target->FindComponentByClass<UJTSHealthComponent>() : nullptr;
			if (!Health || Health->IsDead() || Health->IsInvulnerable() || Target->IsHidden()
				|| FVector::DistSquared(Target->GetActorLocation(), GetOwner()->GetActorLocation()) > FMath::Square(AutomaticTargetRange)) continue;
			const FVector Point = Target->Implements<UJTSMeleeTarget>()
				? IJTSMeleeTarget::Execute_GetMeleeTargetAnchorWorldLocation(Target) : Target->GetActorLocation();
			if (!Module->CanAimAt(Point, Hardpoints[I].bFullRotation)) continue;
			FCollisionQueryParams Query(SCENE_QUERY_STAT(ShipAutomaticTarget), true, GetOwner());
			TArray<AActor*> Attached;
			GetOwner()->GetAttachedActors(Attached);
			for (AActor* Actor : Attached) if (Actor->IsA<AJTSShipWeaponModule>()) Query.AddIgnoredActor(Actor);
			FHitResult Hit;
			if (GetWorld()->LineTraceSingleByChannel(Hit, Module->PitchPivot->GetComponentLocation(), Point, ECC_Visibility, Query)
				&& Hit.GetActor() != Target) continue;
			Module->Aim((Point - Module->PitchPivot->GetComponentLocation()).GetSafeNormal(), Hardpoints[I].bFullRotation);
			if (Module->HasClearShotAt(Target)) Module->FirePulse();
			bHasTarget = true;
			break;
		}
		if (!bHasTarget) Module->Aim(Module->GetActorForwardVector(), Hardpoints[I].bFullRotation);
	}
}
TArray<AJTSShipWeaponModule*> UJTSShipArmamentComponent::GetModules()const
{TArray<AJTSShipWeaponModule*> Out;for(AJTSShipWeaponModule* M:Modules)if(IsValid(M))Out.Add(M);return Out;}
void UJTSShipArmamentComponent::EndPlay(const EEndPlayReason::Type R)
{
	if(UWorld* World=GetWorld())World->GetTimerManager().ClearTimer(UpdateTimer);
	if(GetOwner() && GetOwner()->HasAuthority())for(AJTSShipWeaponModule* M:Modules)if(IsValid(M))M->Destroy();Super::EndPlay(R);
}
void UJTSShipArmamentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps)const
{Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(UJTSShipArmamentComponent,Modules);}
