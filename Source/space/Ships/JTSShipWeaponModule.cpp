#include "space/Ships/JTSShipWeaponModule.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Components/JTSSpacecraftDurabilityComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

AJTSShipWeaponModule::AJTSShipWeaponModule()
{
	bReplicates=true; bAlwaysRelevant=true;
	PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.bStartWithTickEnabled=false;
	WeaponRoot=CreateDefaultSubobject<USceneComponent>(TEXT("WeaponRoot"));SetRootComponent(WeaponRoot);
	Mount=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mount"));Mount->SetupAttachment(WeaponRoot);
	YawPivot=CreateDefaultSubobject<USceneComponent>(TEXT("YawPivot"));YawPivot->SetupAttachment(WeaponRoot);
	Turret=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Turret"));Turret->SetupAttachment(YawPivot);
	PitchPivot=CreateDefaultSubobject<USceneComponent>(TEXT("PitchPivot"));PitchPivot->SetupAttachment(YawPivot);
	Gun=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Gun"));Gun->SetupAttachment(PitchPivot);
	Muzzle=CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));Muzzle->SetupAttachment(Gun);
	Beam=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beam"));Beam->SetupAttachment(WeaponRoot);Beam->SetVisibility(false);
	Impact=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Impact"));Impact->SetupAttachment(WeaponRoot);Impact->SetVisibility(false);
	for(auto* C:{Mount.Get(),Turret.Get(),Gun.Get(),Beam.Get(),Impact.Get()})
	{C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCanEverAffectNavigation(false);}
}
void AJTSShipWeaponModule::Aim(const FVector& Direction,bool bFreeTurret)
{
	if(!HasAuthority() || Direction.ContainsNaN() || Direction.IsNearlyZero())return;
	const FRotator Local=bFixedForward ? FRotator::ZeroRotator : GetActorQuat().UnrotateVector(Direction).Rotation();
	const FRotator NewAim(FMath::Clamp(Local.Pitch,bFreeTurret?MinimumPitch:-LimitedPitch,bFreeTurret?MaximumPitch:LimitedPitch),
		bFreeTurret?Local.Yaw:FMath::Clamp(Local.Yaw,-LimitedYaw,LimitedYaw),0);
	if(!TargetAim.Equals(NewAim,.1f)){TargetAim=NewAim;OnRep_Aim();ForceNetUpdate();}
}
void AJTSShipWeaponModule::OnRep_Aim(){SetActorTickEnabled(true);}
bool AJTSShipWeaponModule::CanAimAt(const FVector& Location, bool bFreeTurret) const
{
	const FVector Direction = Location - (bFixedForward ? Muzzle->GetComponentLocation() : PitchPivot->GetComponentLocation());
	if (Direction.ContainsNaN() || Direction.IsNearlyZero() || Direction.SizeSquared() > FMath::Square(Range)) return false;
	const FRotator Local = GetActorQuat().UnrotateVector(Direction).Rotation();
	const float YawLimit = bFixedForward ? .5f : (bFreeTurret ? 180.f : LimitedYaw);
	const float PitchMin = bFixedForward ? -.5f : (bFreeTurret ? MinimumPitch : -LimitedPitch);
	const float PitchMax = bFixedForward ? .5f : (bFreeTurret ? MaximumPitch : LimitedPitch);
	return FMath::Abs(Local.Yaw) <= YawLimit && Local.Pitch >= PitchMin && Local.Pitch <= PitchMax;
}
bool AJTSShipWeaponModule::HasClearShotAt(const AActor* Target) const
{
	if (!IsValid(Target) || !GetWorld() || !GetOwner()) return false;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(ShipAutomaticFire), true, this);
	// Keep the hull and other players in the trace: neither may be shot through.
	TArray<AActor*> Attached;
	GetOwner()->GetAttachedActors(Attached);
	for (AActor* Actor : Attached) if (Actor->IsA<AJTSShipWeaponModule>()) Query.AddIgnoredActor(Actor);
	FHitResult Hit;
	const FVector Start = Muzzle->GetComponentLocation();
	return GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Muzzle->GetForwardVector() * Range, ECC_Visibility, Query)
		&& Hit.GetActor() == Target;
}
void AJTSShipWeaponModule::FirePulse()
{
	const double Now=GetWorld()->GetTimeSeconds();
	auto* Ship=Cast<AJTSSpacecraftActor>(GetOwner());
	if(!HasAuthority() || !Ship || Now<NextFireTime)return;
    if (const auto* H=Ship->FindComponentByClass<UJTSSpacecraftDurabilityComponent>(); H && H->bDisabled) return;
	NextFireTime=Now+FMath::Max(.08f,FireInterval);
	const FVector Start=Muzzle->GetComponentLocation(),Dir=Muzzle->GetForwardVector();
	FCollisionQueryParams Q(SCENE_QUERY_STAT(ShipLaser),true,this);Q.AddIgnoredActor(Ship);
	// Ignore only our own attached modules, while retaining physical hull occlusion below.
	TArray<AActor*> Attached;Ship->GetAttachedActors(Attached);Q.AddIgnoredActors(Attached);
	FHitResult Hit;
	const bool bHit=GetWorld()->LineTraceSingleByChannel(Hit,Start,Start+Dir*Range,ECC_Visibility,Q);
	// A roof turret may point across its own hull: suppress the shot instead of shooting through it.
	FHitResult HullHit;FCollisionQueryParams HullQ(SCENE_QUERY_STAT(ShipLaserHull),true);
	TArray<UStaticMeshComponent*> HullParts; Ship->GetComponents(HullParts);
	for (auto* Part : HullParts)
		if (Part->GetCollisionEnabled()!=ECollisionEnabled::NoCollision
			&& Part->LineTraceComponent(HullHit,Start,Start+Dir*800,HullQ)) return;
	if(bHit && IsValid(Hit.GetActor()))
		UGameplayStatics::ApplyPointDamage(Hit.GetActor(),Damage,Dir,Hit,Cast<APlayerController>(Ship->GetController()),Ship,nullptr);
	MulticastPulse(bHit?Hit.ImpactPoint:Start+Dir*Range,bHit);
}
void AJTSShipWeaponModule::MulticastPulse_Implementation(FVector_NetQuantize End,bool bHit)
{
	if(GetNetMode()==NM_DedicatedServer)return;
	LocalBeamEnd=GetActorTransform().InverseTransformPosition(End);bPulseHit=bHit;PulseLeft=.13f;SetActorTickEnabled(true);
	if(FireSound)UGameplayStatics::PlaySoundAtLocation(this,FireSound,Muzzle->GetComponentLocation(),.55f,FMath::FRandRange(.94f,1.06f));
}
void AJTSShipWeaponModule::Tick(float Delta)
{
	Super::Tick(Delta);
	const FRotator Yaw=FMath::RInterpConstantTo(YawPivot->GetRelativeRotation(),FRotator(0,TargetAim.Yaw,0),Delta,TurnRate);
	const FRotator Pitch=FMath::RInterpConstantTo(PitchPivot->GetRelativeRotation(),FRotator(TargetAim.Pitch,0,0),Delta,TurnRate);
	YawPivot->SetRelativeRotation(Yaw);PitchPivot->SetRelativeRotation(Pitch);
	PulseLeft=FMath::Max(0.f,PulseLeft-Delta);
	Gun->SetRelativeLocation(FVector(-RecoilTravel*(PulseLeft/.13f),0,0));
	Beam->SetVisibility(PulseLeft>0);Impact->SetVisibility(PulseLeft>0 && bPulseHit);
	if(PulseLeft>0)
	{
		const FVector A=Muzzle->GetComponentLocation(),B=GetActorTransform().TransformPosition(LocalBeamEnd),D=B-A;
		Beam->SetWorldLocation((A+B)*.5);Beam->SetWorldRotation(FRotationMatrix::MakeFromZ(D).Rotator());
		// Blueprint assigns a unit cylinder (radius .5, height 1) to preserve a physical beam width.
		Beam->SetWorldScale3D(FVector(BeamRadius*2,BeamRadius*2,D.Size()));
		Impact->SetWorldLocation(B);Impact->SetWorldScale3D(FVector(10*(PulseLeft/.13f)));
	}
	if(PulseLeft<=0 && Yaw.Equals(FRotator(0,TargetAim.Yaw,0),.05f) && Pitch.Equals(FRotator(TargetAim.Pitch,0,0),.05f))SetActorTickEnabled(false);
}
void AJTSShipWeaponModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps)const
{Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(AJTSShipWeaponModule,TargetAim);}
