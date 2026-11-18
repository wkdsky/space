#include "space/Components/JTSShipCabinCouplingComponent.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/Ships/JTSSpacecraftCollisionQuery.h"
#include "Components/SceneComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UJTSShipCabinCouplingComponent::UJTSShipCabinCouplingComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

FJTSCabinCouplingMotion UJTSShipCabinCouplingComponent::EvaluateMotion(float P, bool bCapture)
{
	P=FMath::Clamp(P,0.f,1.f);
	const auto Ease=[P](float A,float B){return FMath::SmoothStep(A,B,P);};
	FJTSCabinCouplingMotion M;
	if (!bCapture)
	{
		M.Lock=1-Ease(0,.12f);
		M.SideExtension=1-Ease(.12f,.38f);
		M.RearExtension=Ease(.40f,.66f)*(1-Ease(.72f,.96f));
		M.CabinTravel=.12f*Ease(.40f,.66f)+.88f*Ease(.66f,1);
	}
	else
	{
		M.CabinTravel=1-.88f*Ease(0,.48f)-.12f*Ease(.52f,.72f);
		M.RearExtension=Ease(.35f,.48f)*(1-Ease(.52f,.72f));
		M.SideExtension=Ease(.74f,.94f);
		M.Lock=Ease(.94f,1);
	}
	return M;
}

double UJTSShipCabinCouplingComponent::ServerTime() const
{
	const auto* GS=GetWorld()->GetGameState();
	return GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

void UJTSShipCabinCouplingComponent::BindRig()
{
	TInlineComponentArray<USceneComponent*> Components(GetOwner());
	for (auto* C:Components) if(C->GetFName()==CabinRootName) CabinRoot=C;
	if(!CabinRoot.IsValid()) return;
	for(auto* C:Components)
		if(C!=CabinRoot.Get() && CabinComponentNames.Contains(C->GetFName()) && C->GetAttachParent()!=CabinRoot.Get())
			C->AttachToComponent(CabinRoot.Get(),FAttachmentTransformRules::KeepWorldTransform);
	Links.Reset();
	for(const auto& Definition:SideLinks)
		for(auto* C:Components) if(C->GetFName()==Definition.ComponentName)
		{
			FLinkBinding& B=Links.AddDefaulted_GetRef(); B.Component=C;B.Locked=C->GetRelativeLocation();
			B.Offset=Definition.RetractedOffset;B.bLockPin=Definition.bLockPin;
		}
	for(auto* C:Components) if(C->GetFName()==RearPistonName){RearPiston=C;RearLocked=C->GetRelativeLocation();}
}

void UJTSShipCabinCouplingComponent::BeginPlay()
{
	Super::BeginPlay();BindRig();ApplyPose();
	SetComponentTickEnabled(Snapshot.State!=EJTSCabinCouplingState::Docked && Snapshot.State!=EJTSCabinCouplingState::Detached);
}

bool UJTSShipCabinCouplingComponent::IsPathClear(const FTransform& From,const FTransform& To) const
{
	if(ClearanceExtent.GetMin()<=0) return false;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(CabinCapture),false,GetOwner());
	JTSSpacecraftCollision::IgnoreSelfAndAttachments(Q,GetOwner());
	return !GetWorld()->SweepTestByChannel(From.TransformPosition(ClearanceCentre),To.TransformPosition(ClearanceCentre),
		To.GetRotation(),ECC_WorldStatic,FCollisionShape::MakeBox(ClearanceExtent*To.GetScale3D().GetAbs()),Q);
}

bool UJTSShipCabinCouplingComponent::ReleaseCabin()
{
	if(!GetOwner()->HasAuthority() || !IsCabinDocked() || !CabinRoot.IsValid() || EjectionOffset.IsNearlyZero()) return false;
	const auto* Ship=Cast<AJTSSpacecraftActor>(GetOwner());
	if(Ship && (Ship->IsLanded() || Ship->GetFlightState()!=EJTSSpacecraftFlightState::Flying)) return false;
	FTransform End=GetOwner()->GetActorTransform();End.AddToTranslation(End.TransformVector(EjectionOffset));
	if(!IsPathClear(CabinRoot->GetComponentTransform(),End)) return false;
	Snapshot.State=EJTSCabinCouplingState::Releasing;Snapshot.StartTime=ServerTime();
	OnRep_Snapshot();GetOwner()->ForceNetUpdate();return true;
}

bool UJTSShipCabinCouplingComponent::CaptureCabin()
{
	if(!GetOwner()->HasAuthority() || Snapshot.State!=EJTSCabinCouplingState::Detached || !CabinRoot.IsValid()) return false;
	const FTransform Dock=GetOwner()->GetActorTransform();
	if(FVector::Dist(CabinRoot->GetComponentLocation(),Dock.GetLocation())>CaptureRange
		|| FMath::RadiansToDegrees(CabinRoot->GetComponentQuat().AngularDistance(Dock.GetRotation()))>45
		|| !IsPathClear(CabinRoot->GetComponentTransform(),Dock)) return false;
	Snapshot.FreePose=CabinRoot->GetComponentTransform();Snapshot.State=EJTSCabinCouplingState::Capturing;
	Snapshot.StartTime=ServerTime();OnRep_Snapshot();GetOwner()->ForceNetUpdate();return true;
}

bool UJTSShipCabinCouplingComponent::SetDetachedCabinTransform(FTransform Pose)
{
	if(!GetOwner()->HasAuthority() || Snapshot.State!=EJTSCabinCouplingState::Detached || Pose.ContainsNaN()
		|| !Pose.GetScale3D().Equals(GetOwner()->GetActorScale3D(),.001f) || !IsPathClear(Snapshot.FreePose,Pose)) return false;
	Snapshot.FreePose=Pose;ApplyPose();GetOwner()->ForceNetUpdate();return true;
}

FTransform UJTSShipCabinCouplingComponent::GetCabinWorldTransform() const
{return CabinRoot.IsValid()?CabinRoot->GetComponentTransform():FTransform::Identity;}

void UJTSShipCabinCouplingComponent::OnRep_Snapshot()
{
	ApplyPose();
	SetComponentTickEnabled(Snapshot.State==EJTSCabinCouplingState::Releasing || Snapshot.State==EJTSCabinCouplingState::Capturing);
}

void UJTSShipCabinCouplingComponent::ApplyPose()
{
	if(!CabinRoot.IsValid())return;
	const bool Capture=Snapshot.State==EJTSCabinCouplingState::Capturing;
	const bool Transition=Capture || Snapshot.State==EJTSCabinCouplingState::Releasing;
	const float P=Transition?FMath::Clamp(float((ServerTime()-Snapshot.StartTime)/FMath::Max(.1f,Capture?CaptureDuration:ReleaseDuration)),0.f,1.f):
		(Snapshot.State==EJTSCabinCouplingState::Detached?1.f:0.f);
	const auto Motion=EvaluateMotion(P,Capture);
	const FTransform Dock=GetOwner()->GetActorTransform();
	if(Snapshot.State==EJTSCabinCouplingState::Detached)
	{
		CabinRoot->SetAbsolute(true,true,true);CabinRoot->SetWorldTransform(Snapshot.FreePose);
	}
	else if(Capture)
	{
		FTransform Pose;Pose.Blend(Dock,Snapshot.FreePose,Motion.CabinTravel);
		CabinRoot->SetAbsolute(true,true,true);CabinRoot->SetWorldTransform(Pose);
	}
	else
	{
		CabinRoot->SetAbsolute(false,false,false);
		CabinRoot->SetRelativeTransform(FTransform(EjectionOffset*Motion.CabinTravel));
	}
	for(const auto& B:Links) if(B.Component.IsValid())
		B.Component->SetRelativeLocation(B.Locked+B.Offset*(1-(B.bLockPin?Motion.Lock:Motion.SideExtension)));
	if(RearPiston.IsValid()) RearPiston->SetRelativeLocation(RearLocked+RearPistonTravel*Motion.RearExtension);
}

void UJTSShipCabinCouplingComponent::TickComponent(float D,ELevelTick T,FActorComponentTickFunction* F)
{
	Super::TickComponent(D,T,F);
	if(Snapshot.State!=EJTSCabinCouplingState::Releasing && Snapshot.State!=EJTSCabinCouplingState::Capturing)return;
	ApplyPose();
	if(!GetOwner()->HasAuthority()) return;
	const bool Capture=Snapshot.State==EJTSCabinCouplingState::Capturing;
	if(ServerTime()-Snapshot.StartTime < FMath::Max(.1f,Capture?CaptureDuration:ReleaseDuration))return;
	if(Capture) Snapshot.State=EJTSCabinCouplingState::Docked;
	else {Snapshot.FreePose=GetCabinWorldTransform();Snapshot.State=EJTSCabinCouplingState::Detached;}
	OnRep_Snapshot();GetOwner()->ForceNetUpdate();
}

void UJTSShipCabinCouplingComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps)const
{Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(UJTSShipCabinCouplingComponent,Snapshot);}
