#include "space/Components/JTSShipDockComponent.h"
#include "space/Components/JTSSpacecraftDurabilityComponent.h"
#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"
#include "space/Ships/JTSShipDockActor.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/World/JTSSpaceWorldManager.h"
#include "space/Core/JTSGameState.h"

UJTSShipDockComponent::UJTSShipDockComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.05f;
}
void UJTSShipDockComponent::BeginPlay()
{
	Super::BeginPlay();
	LastTransform = GetOwner()->GetActorTransform();
	if (GetOwner()->HasAuthority())
	{
		Terminal = SpawnDefinition(TerminalDefinition);
		RestoreInstalledIds(InitialInstalledIds);
	}
	else SetComponentTickEnabled(false);
}
void UJTSShipDockComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GetOwner()->HasAuthority()) for (AJTSShipDockActor* Dock : Docks) if (IsValid(Dock)) Dock->Destroy();
	if (GetOwner()->HasAuthority() && IsValid(Terminal)) Terminal->Destroy();
	Super::EndPlay(Reason);
}
void UJTSShipDockComponent::SpawnDock(FName Id)
{
	const auto* Found = Catalog.FindByPredicate([Id](const UJTSShipDockDefinition* Def) { return Def && Def->ModuleId == Id; });
	if (Found) if (AJTSShipDockActor* Dock = SpawnDefinition(*Found)) Docks.Add(Dock);
}
AJTSShipDockActor* UJTSShipDockComponent::SpawnDefinition(const UJTSShipDockDefinition* Def)
{
	AJTSSpacecraftActor* Ship = Cast<AJTSSpacecraftActor>(GetOwner());
	if (!Def || !Def->ActorClass || !Ship) return nullptr;
	FActorSpawnParameters Params;
	Params.Owner = Ship;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AJTSShipDockActor* Dock = GetWorld()->SpawnActor<AJTSShipDockActor>(Def->ActorClass,
		Def->MountTransform * Ship->GetActorTransform(), Params);
	if (!Dock) return nullptr;
	USceneComponent* Parent=Ship->GetRootComponent();
	for(auto* C:TInlineComponentArray<USceneComponent*>(Ship))
		if(C->GetFName()==Def->ParentComponentName){Parent=C;break;}
	Dock->AttachToComponent(Parent, FAttachmentTransformRules::KeepWorldTransform);
	Dock->InitializeDock(Ship);
	return Dock;
}
void UJTSShipDockComponent::RestoreInstalledIds(const TArray<FName>& Ids)
{
	if (!GetOwner()->HasAuthority()) return;
	for (AJTSShipDockActor* Dock : Docks) if (IsValid(Dock)) Dock->Destroy();
	Docks.Reset(); InstalledIds.Reset();
	TArray<FName> EffectiveIds=Ids;
	for(const UJTSShipDockDefinition* Def:Catalog) if(Def && Def->bRequired) EffectiveIds.AddUnique(Def->ModuleId);
	for (FName Id : EffectiveIds)
	{
		if (InstalledIds.Contains(Id) || !Catalog.ContainsByPredicate([Id](const UJTSShipDockDefinition* Def)
			{ return Def && Def->ModuleId == Id && Def->ActorClass; })) continue;
		const int32 Before = Docks.Num(); SpawnDock(Id);
		if (Docks.Num() > Before) InstalledIds.Add(Id);
	}
}
void UJTSShipDockComponent::Persist() const
{
	if (AJTSSpacecraftActor* Ship = Cast<AJTSSpacecraftActor>(GetOwner())) Ship->PersistMaterialConfiguration();
}
bool UJTSShipDockComponent::InstallDock(FName Id)
{
	if (!GetOwner()->HasAuthority() || !IsParked() || InstalledIds.Contains(Id)) return false;
	if (!Catalog.ContainsByPredicate([Id](const UJTSShipDockDefinition* Def) { return Def && Def->ModuleId == Id && Def->ActorClass; })) return false;
	const int32 Before = Docks.Num();
	SpawnDock(Id);
	if (Before == Docks.Num()) return false;
	InstalledIds.Add(Id); StableSeconds = 0; Persist(); return true;
}
bool UJTSShipDockComponent::RemoveDock(FName Id)
{
	if (!GetOwner()->HasAuthority() || !IsParked()) return false;
	if(Catalog.ContainsByPredicate([Id](const UJTSShipDockDefinition* Def){return Def && Def->ModuleId==Id && Def->bRequired;})) return false;
	const int32 Index = InstalledIds.IndexOfByKey(Id);
	if (Index == INDEX_NONE) return false;
	if(Docks.IsValidIndex(Index) && IsValid(Docks[Index]) && !Docks[Index]->CanRemoveDock()) return false;
	if (Docks.IsValidIndex(Index)) { if (IsValid(Docks[Index])) Docks[Index]->Destroy(); Docks.RemoveAt(Index); }
	InstalledIds.RemoveAt(Index); Persist(); return true;
}
bool UJTSShipDockComponent::IsParked() const
{
	const AJTSSpacecraftActor* Ship = Cast<AJTSSpacecraftActor>(GetOwner());
	const AJTSGameState* State = GetWorld()->GetGameState<AJTSGameState>();
	return Ship && (AJTSSpaceWorldManager::FindSpaceWorldManager(Ship) ? Ship->IsLanded() : !(State && State->IsLaunching()));
}
bool UJTSShipDockComponent::CanUseService(EJTSShipDockService Service, APawn* Pawn) const
{
    if (const auto* H=GetOwner()->FindComponentByClass<UJTSSpacecraftDurabilityComponent>(); H && H->bDisabled) return false;
	if (IsValid(Terminal) && Terminal->GetService() == Service && Terminal->CanUseDock(Pawn)) return true;
	for (const AJTSShipDockActor* Dock : Docks)
		if (IsValid(Dock) && Dock->GetService() == Service && Dock->CanUseDock(Pawn)) return true;
	return false;
}
bool UJTSShipDockComponent::AreAllStowed() const
{
	for (const AJTSShipDockActor* Dock : Docks) if (IsValid(Dock) && !Dock->IsStowed()) return false;
	return true;
}
void UJTSShipDockComponent::NotifyTakeoff()
{
	if (!GetOwner()->HasAuthority()) return;
	StableSeconds = 0;
	for (AJTSShipDockActor* Dock : Docks) if (IsValid(Dock)) Dock->SetDeployed(false);
}
void UJTSShipDockComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Function)
{
	Super::TickComponent(DeltaTime, TickType, Function);
	AJTSSpacecraftActor* Ship = Cast<AJTSSpacecraftActor>(GetOwner());
	if (!Ship || !Ship->HasAuthority()) return;
	const FTransform Now = Ship->GetActorTransform();
	const bool bStable = Now.GetLocation().Equals(LastTransform.GetLocation(), 0.5f)
		&& Now.GetRotation().AngularDistance(LastTransform.GetRotation()) < FMath::DegreesToRadians(0.1f);
	const bool bParked = IsParked();
	StableSeconds = bParked && bStable ? StableSeconds + DeltaTime : 0;
	LastTransform = Now;
	// Once deployment starts, minor parked-body adjustments must not reverse/restart it.
	// Only leaving the parked state sends a retract signal.
	if (!bParked || StableSeconds >= SettleDuration)
		for (AJTSShipDockActor* Dock : Docks) if (IsValid(Dock)) Dock->SetDeployed(bParked);
}
void UJTSShipDockComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UJTSShipDockComponent, InstalledIds); DOREPLIFETIME(UJTSShipDockComponent, Docks);
	DOREPLIFETIME(UJTSShipDockComponent, Terminal);
}
