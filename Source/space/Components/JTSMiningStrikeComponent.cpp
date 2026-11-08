#include "JTSMiningStrikeComponent.h"
#include "JTSMiningImpactComponent.h"
#include "JTSNestBlockadeComponent.h"
#include "JTSMeleeComponent.h"
#include "JTSHealthComponent.h"
#include "JTSRangedWeaponComponent.h"
#include "JTSInventoryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "space/Items/JTSItemDefinition.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Player/JTSCharacter.h"
#include "space/Interaction/JTSMeleeTarget.h"

namespace
{
	// Interaction bounds cover the whole mouth, including empty air and nearby defenders.
	// They may select an E interaction, but only actual geometry can obstruct a tool contact.
	bool TracePhysicalVisibility(UWorld* World, FHitResult& Hit, const FVector& Start, const FVector& End,
		FCollisionQueryParams& Query, float Radius = 0.f)
	{
		for (int32 Skipped = 0; Skipped < 16; ++Skipped)
		{
			const bool bHit = Radius > 0.f
				? World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(Radius), Query)
				: World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Query);
			if (!bHit) return false;
			if (!Hit.GetComponent() || !Hit.GetComponent()->ComponentHasTag(TEXT("InteractionProxy"))) return true;
			Query.AddIgnoredComponent(Hit.GetComponent());
		}
		return true;
	}

	void IgnoreTargetVolumes(FCollisionQueryParams& Query, AActor* Target, const UPrimitiveComponent* Surface)
	{
		TArray<UPrimitiveComponent*> Components;
		Target->GetComponents(Components);
		for (const auto* Component : Components)
			if (Component != Surface) Query.AddIgnoredComponent(Component);
	}
}

UJTSMiningStrikeComponent::UJTSMiningStrikeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetIsReplicatedByDefault(true);
}
const UJTSItemDefinition* UJTSMiningStrikeComponent::GetToolDefinition() const
{
	const auto* Inventory = GetOwner()->FindComponentByClass<UJTSInventoryComponent>();
	const auto* Item = Inventory ? UJTSItemDefinitionLibrary::GetItemDefinition(this, Inventory->GetActiveItemId()) : nullptr;
	return Item && Item->bDirectedMiningSwing && Item->MiningWork > 0 ? Item : nullptr;
}
bool UJTSMiningStrikeComponent::IsDirectedToolEquipped() const { return GetToolDefinition() != nullptr; }
void UJTSMiningStrikeComponent::GetViewRay(FVector& Origin, FVector& Direction) const
{
    if (const auto* View = GetOwner()->FindComponentByClass<UJTSRangedWeaponComponent>())
        if (View->GetCameraRay(Origin, Direction)) return;
    const auto* Pawn = Cast<APawn>(GetOwner());
    Origin = Pawn ? Pawn->GetPawnViewLocation() : GetOwner()->GetActorLocation();
    Direction = Pawn ? Pawn->GetBaseAimRotation().Vector() : GetOwner()->GetActorForwardVector();
}
FTransform UJTSMiningStrikeComponent::GetModelGripTransform() const
{
	const auto* Item = GetToolDefinition();
	return Item ? Item->HeldPresentation.GripTransform : FTransform::Identity;
}

FVector UJTSMiningStrikeComponent::GetPresentationContact() const
{
	return PresentationTarget.IsValid() && !bContactResolved
		? PresentationTarget->GetActorTransform().TransformPosition(TargetLocalContact) : ContactPoint;
}

bool UJTSMiningStrikeComponent::GetContactPose(const FVector& Point, FVector& Shoulder, FVector& Grip, FQuat& Rotation, bool bClampReach) const
{
	const auto* Item = GetToolDefinition();
	if (!Item) return false;
	const FVector Up = GetOwner()->GetActorUpVector();
	const FVector Forward = GetOwner()->GetActorForwardVector();
	const FVector Right = GetOwner()->GetActorRightVector();
	Shoulder = GetOwner()->GetActorTransform().TransformPositionNoScale(ElbowReferenceOffset) + Up * UpperArmReach;
	const FVector TipOffset = GetModelGripTransform().InverseTransformPosition(Item->HeldPresentation.MuzzleTransform.GetLocation());
	const float ToolLength = TipOffset.Size();
	const float ArmLength = UpperArmReach + ForearmReach - 1.f;
	const FVector Delta = Point - Shoulder;
	const float Minimum = FMath::Abs(ToolLength - ArmLength) + .1f;
	const float Maximum = ToolLength + ArmLength - .1f;
	if (!bClampReach && (Delta.Size() < Minimum || Delta.Size() > Maximum)) return false;
	const FVector Along = Delta.GetSafeNormal(SMALL_NUMBER, Forward);
	const float Distance = FMath::Clamp(float(Delta.Size()), Minimum, Maximum);
	const float A = (ArmLength * ArmLength - ToolLength * ToolLength + Distance * Distance) / (2 * Distance);
	const float B = FMath::Sqrt(FMath::Max(0.f, ArmLength * ArmLength - A * A));
	// The hand goes forward and outward, rather than folding a locked elbow through the chest.
	const FVector Pole = FVector::VectorPlaneProject(Forward + Up * .45f + Right * .25f, Along).GetSafeNormal(SMALL_NUMBER, Right);
	Grip = Shoulder + Along * A + Pole * B;
	const FVector TipDirection = (Shoulder + Along * Distance - Grip).GetSafeNormal();
	const FVector ToolRight = FVector::CrossProduct(Up, TipDirection).GetSafeNormal(SMALL_NUMBER, Right);
	Rotation = FRotationMatrix::MakeFromXY(TipDirection, ToolRight).ToQuat()
		* FQuat::FindBetweenNormals(TipOffset.GetSafeNormal(), FVector::ForwardVector);
	return true;
}

void UJTSMiningStrikeComponent::KeepToolOutsideBody(FVector& Grip, const FQuat& Rotation) const
{
	const auto* Item = GetToolDefinition();
	if (!Item) return;
	const FVector Forward = GetOwner()->GetActorForwardVector();
	const FVector TipOffset = GetModelGripTransform().InverseTransformPosition(Item->HeldPresentation.MuzzleTransform.GetLocation());
	// Protect the handle, its pommel and both sides of the pick head throughout the arc.
	const FVector Shaft = Rotation.GetAxisX();
	const float Rear = FMath::Min(-12.f * FVector::DotProduct(Shaft, Forward),
		float(FVector::DotProduct(Rotation.RotateVector(TipOffset), Forward)));
	const float Depth = FVector::DotProduct(Grip - GetOwner()->GetActorLocation(), Forward);
	Grip += Forward * FMath::Max(0.f, TorsoClearance - Depth - FMath::Min(0.f, Rear));
}

bool UJTSMiningStrikeComponent::IsReachable(const FVector& Point) const
{
    const auto* Item = GetToolDefinition();
    if (!Item || FVector::DistSquared(Point,GetOwner()->GetActorLocation())>FMath::Square(Item->MiningReach)) return false;
    const FVector Delta=Point-GetOwner()->GetActorLocation();
    const FVector Planar=FVector::VectorPlaneProject(Delta,GetOwner()->GetActorUpVector());
    FVector ViewOrigin, ViewDirection;
    GetViewRay(ViewOrigin, ViewDirection);
    const FVector Forward = bSwingActive ? GetOwner()->GetActorForwardVector()
        : FVector::VectorPlaneProject(ViewDirection, GetOwner()->GetActorUpVector()).GetSafeNormal(SMALL_NUMBER, GetOwner()->GetActorForwardVector());
    if (FVector::DotProduct(Delta,Forward)<-12.f) return false;
    if (Planar.SizeSquared()>FMath::Square(25.f) && FVector::DotProduct(Planar.GetSafeNormal(),Forward)<.17f) return false;
    // Gameplay reach covers the front hemisphere, including the ground beside the feet.
    // The visual arm solves toward this point within its own joint limits.
    return true;
}
bool UJTSMiningStrikeComponent::FindWorkContact(AActor* Target, FHitResult& OutHit) const
{
	const auto* Impact = IsValid(Target) ? Target->FindComponentByClass<UJTSMiningImpactComponent>() : nullptr;
	auto* Mesh = Impact ? Impact->GetSurfaceMesh() : nullptr;
	if (!Mesh) return false;
	const FVector Up = GetOwner()->GetActorUpVector();
	const FVector Start = GetOwner()->GetActorLocation() + Up * 35;
	const FVector Center = Mesh->Bounds.Origin;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(MiningWorkContact), true, GetOwner());
	IgnoreTargetVolumes(Query, Target, Mesh);
	const FVector Forward=Mesh->GetForwardVector(), Right=Mesh->GetRightVector();
	const float Radius=Mesh->Bounds.SphereRadius;
	float BestDistance=TNumericLimits<float>::Max();
	bool bFound=false;
	// Prefer the near soil shoulder/lip. A ray through the empty mouth finds the unreachable back wall.
	for (const float Depth : {-.5f, -.25f, 0.f})
	for (const float Side : {0.f, -.18f, .18f})
	for (const float Height : {-.12f, .1f})
	{
		FHitResult Hit;
		if (Mesh->LineTraceComponent(Hit, Start, Center+Radius*(Forward*Depth+Right*Side+Up*Height), Query) && IsReachable(Hit.ImpactPoint))
		{
			FHitResult Blocker;
			if (TracePhysicalVisibility(GetWorld(), Blocker, Start, Hit.ImpactPoint, Query)
				&& Blocker.GetActor() != Target) continue;
			const float Distance=FVector::DistSquared(GetOwner()->GetActorLocation(),Hit.ImpactPoint);
			if (Distance<BestDistance) { BestDistance=Distance; OutHit=Hit; bFound=true; }
		}
	}
	return bFound;
}
bool UJTSMiningStrikeComponent::AcquireContact(FHitResult& OutHit) const
{
	auto* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn) return false;
	FVector Camera, Direction; GetViewRay(Camera, Direction);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(MiningAim), true, Pawn);
	TArray<AActor*> Attached; Pawn->GetAttachedActors(Attached, true, true);
	Query.AddIgnoredActors(Attached);
	const FVector Hand=GetOwner()->GetActorTransform().TransformPositionNoScale(ElbowReferenceOffset);
	auto Accept=[&](const FHitResult& Hit, FCollisionQueryParams ContactQuery)
	{
		AActor* Target=Hit.GetActor();
		if (!IsValid(Target) || !Target->Implements<UJTSMeleeTarget>()
			|| !IJTSMeleeTarget::Execute_CanReceiveMeleeHit(Target,Pawn) || !IsReachable(Hit.ImpactPoint)) return false;
        // Damage reach is an explicit gameplay rule, independent of a rendered arm pose.
        // A server never rejects a valid hit because a cosmetic IK triangle cannot close.
		FHitResult Blocker;
		return !TracePhysicalVisibility(GetWorld(),Blocker,Hand,Hit.ImpactPoint,ContactQuery) || Blocker.GetActor()==Target;
	};
	FHitResult Aim, BodyAim;
	const FVector RayEnd = Camera + Direction * 1400;
	const bool bSurfaceHit = TracePhysicalVisibility(GetWorld(), Aim, Camera, RayEnd, Query);
	FCollisionQueryParams BodyQuery = Query;
	BodyQuery.bTraceComplex = false;
	const bool bBodyHit = TracePhysicalVisibility(GetWorld(), BodyAim, Camera, RayEnd, BodyQuery);
	if (bBodyHit && (!bSurfaceHit || BodyAim.Distance <= Aim.Distance + 1.f)
		&& Accept(BodyAim, BodyQuery)) { OutHit = BodyAim; return true; }
	if (bSurfaceHit && (!bBodyHit || Aim.Distance <= BodyAim.Distance + 1.f)
		&& Accept(Aim, Query)) { OutHit = Aim; return true; }
	// A compact hull catches a grazing pick-head contact on small, fast combat bodies.
	// The world trace still stops at the first wall; the hand path is checked by Accept.
	if (ContactSweepRadius > 0.f && TracePhysicalVisibility(GetWorld(), BodyAim, Camera, RayEnd, BodyQuery, ContactSweepRadius)
		&& Accept(BodyAim, BodyQuery)) { OutHit = BodyAim; return true; }
	const auto* Item=GetToolDefinition();
	if (!Item || AimAssistDegrees<=0) return false;
	TArray<FOverlapResult> Nearby;
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic); Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams OverlapQuery=Query;
	OverlapQuery.bTraceComplex=false; // Creature bodies use simple collision, unlike authored mining surfaces.
	GetWorld()->OverlapMultiByObjectType(Nearby,GetOwner()->GetActorLocation(),FQuat::Identity,Objects,
		FCollisionShape::MakeSphere(Item->MiningReach),OverlapQuery);
	TSet<AActor*> Visited;
	float BestScore=TNumericLimits<float>::Max();
	bool bFound=false;
	for (const FOverlapResult& Overlap : Nearby)
	{
		AActor* Target=Overlap.GetActor();
		if (!IsValid(Target) || Visited.Contains(Target)) continue;
		Visited.Add(Target);
		const auto* Impact=Target->FindComponentByClass<UJTSMiningImpactComponent>();
		UPrimitiveComponent* Mesh=Impact?Impact->GetSurfaceMesh():nullptr;
		// Combat bodies have no mining-feedback component. Select their real blocking shape.
		if (!Mesh && Target->FindComponentByClass<UJTSHealthComponent>())
		{
			TArray<UPrimitiveComponent*> Primitives;
			Target->GetComponents(Primitives);
			for (auto* Primitive : Primitives)
				if (Primitive->IsQueryCollisionEnabled() && Primitive->GetCollisionResponseToChannel(ECC_Visibility)==ECR_Block)
				{ Mesh=Primitive; break; }
		}
		if (!Mesh) continue;
		const FVector ToCenter=Mesh->Bounds.Origin-Camera;
		const float Depth=FVector::DotProduct(ToCenter,Direction);
		const FVector BodyDelta=Mesh->Bounds.Origin-Pawn->GetActorLocation();
		const FVector Planar=FVector::VectorPlaneProject(BodyDelta,Pawn->GetActorUpVector());
		const bool bCloseCreature=!Impact && Target->FindComponentByClass<UJTSHealthComponent>()
			&& BodyDelta.SizeSquared()<=FMath::Square(CloseCombatAssistDistance+Mesh->Bounds.SphereRadius)
			&& FVector::DotProduct(Planar.GetSafeNormal(),Pawn->GetActorForwardVector())
				>=FMath::Cos(FMath::DegreesToRadians(CloseCombatAssistHalfAngle));
		if (!bCloseCreature && (Depth<=0 || FVector::VectorPlaneProject(ToCenter,Direction).Size()
			> Mesh->Bounds.SphereRadius+Depth*FMath::Tan(FMath::DegreesToRadians(AimAssistDegrees)))) continue;
		FCollisionQueryParams ContactQuery=Query;
		ContactQuery.bTraceComplex=Impact!=nullptr;
		IgnoreTargetVolumes(ContactQuery,Target,Mesh);
		FHitResult Contact,Blocker;
		const FVector TraceOrigin=bCloseCreature?Hand:Camera;
		if (!Mesh->LineTraceComponent(Contact,TraceOrigin,Mesh->Bounds.Origin,ContactQuery) || !Accept(Contact,ContactQuery)) continue;
		if (TracePhysicalVisibility(GetWorld(),Blocker,TraceOrigin,Contact.ImpactPoint,ContactQuery) && Blocker.GetActor()!=Target) continue;
		const FVector ToPoint=Contact.ImpactPoint-Camera;
		const float Score=bCloseCreature ? 1.f+BodyDelta.Size()/FMath::Max(1.f,CloseCombatAssistDistance)
			: FVector::VectorPlaneProject(ToPoint,Direction).Size()/FMath::Max(1.f,Depth);
		if (Score<BestScore) { BestScore=Score; OutHit=Contact; bFound=true; }
	}
	return bFound;
}
void UJTSMiningStrikeComponent::BeginSwing(AActor* WorkTarget)
{
	if (!GetOwner()->HasAuthority() || !GetToolDefinition()) return;
	ExplicitWorkTarget = WorkTarget;
	PendingContact = FHitResult();
	const bool bTargeted = WorkTarget ? FindWorkContact(WorkTarget, PendingContact) : AcquireContact(PendingContact);
    FVector Camera,Direction; GetViewRay(Camera,Direction);
    const FVector Up=GetOwner()->GetActorUpVector(), Forward=GetOwner()->GetActorForwardVector();
    // A miss still follows the crosshair. Keep the working arc in front of the torso.
    const float Pitch=FMath::Clamp(FVector::DotProduct(Direction,Up),-.85f,.65f);
    FVector Planar=FVector::VectorPlaneProject(Direction,Up).GetSafeNormal(SMALL_NUMBER,Forward);
    if (FVector::DotProduct(Planar,Forward)<.35f)
        Planar=(Planar+Forward).GetSafeNormal(SMALL_NUMBER,Forward);
    Direction=(Planar*FMath::Sqrt(1-Pitch*Pitch)+Up*Pitch).GetSafeNormal();
    const FVector Elbow=GetOwner()->GetActorTransform().TransformPositionNoScale(ElbowReferenceOffset);
    const FVector End=bTargeted?PendingContact.ImpactPoint:Elbow+Direction*95;

	MulticastBeginSwing(bTargeted ? PendingContact.GetActor() : nullptr, End, bTargeted ? PendingContact.ImpactNormal : -Forward, bTargeted);
}
void UJTSMiningStrikeComponent::MulticastBeginSwing_Implementation(AActor* Target, FVector_NetQuantize Point, FVector_NetQuantizeNormal Normal, bool bTargeted)
{
	bFirstStroke = !bSwingActive;
	SwingStart = GetWorld()->GetTimeSeconds();
	LastContactSample = 0;
	ContactPoint = Point; ContactNormal = Normal;
	PresentationTarget = Target;
	TargetLocalContact = IsValid(Target) ? Target->GetActorTransform().InverseTransformPosition(Point) : FVector::ZeroVector;
	SetComponentTickEnabled(GetOwner()->HasAuthority());
	SwingUp = GetOwner()->GetActorUpVector();
	SwingForward = FVector::VectorPlaneProject(Point - GetOwner()->GetActorLocation(), SwingUp).GetSafeNormal();
	if (SwingForward.IsNearlyZero()) SwingForward = GetOwner()->GetActorForwardVector();
	bSwingActive = true; bContactResolved = false; bHasContact = bTargeted;
	if (GetWorld()->GetNetMode() != NM_DedicatedServer)
		if (const auto* Item = GetToolDefinition(); Item && !Item->MiningSwingSound.IsNull())
			UGameplayStatics::PlaySoundAtLocation(this, Item->MiningSwingSound.LoadSynchronous(), GetOwner()->GetActorLocation(),
				FRotator::ZeroRotator, .38f, FMath::FRandRange(.94f,1.05f), 0.f, Item->MiningSwingAttenuation.LoadSynchronous());
}
bool UJTSMiningStrikeComponent::GetGripPose(FVector& Grip, FQuat& Rotation, FVector& Tip, float& Load) const
{
    const auto* Item=GetToolDefinition();
    if (!Item || !GetWorld()) return false;
    const FVector Up=GetOwner()->GetActorUpVector(), Forward=GetOwner()->GetActorForwardVector();
    const FVector Right=GetOwner()->GetActorRightVector();
    FVector Elbow=GetOwner()->GetActorTransform().TransformPositionNoScale(ElbowReferenceOffset);
    const FVector IdleGrip=GetOwner()->GetActorTransform().TransformPositionNoScale(IdleGripOffset);
    const FQuat IdleRotation=FRotationMatrix::MakeFromXY(Up,Right).ToQuat();
    const FVector TipOffset=GetModelGripTransform().InverseTransformPosition(Item->HeldPresentation.MuzzleTransform.GetLocation());
    Grip=IdleGrip; Rotation=IdleRotation; Load=0;
    const double Now=GetWorld()->GetTimeSeconds();
    const float Release=FMath::Clamp(float(Now-ReturnStart)/.18f,0.f,1.f);
    if (bSwingActive || Release<1)
    {
        FVector ContactGrip; FQuat ContactRotation;
        const FVector WorkingPoint=GetPresentationContact();
        if (!GetContactPose(WorkingPoint,Elbow,ContactGrip,ContactRotation,true)) return false;
        const float Phase=FMath::Clamp(float(Now-SwingStart)/FMath::Max(.1f,Item->MiningSwingDuration),0.f,1.f);
        const float Contact=Item->MiningContactFraction;
        float Lift=ReboundDegrees;
        if (bSwingActive)
        {
            if (Phase<.15f) Lift=FMath::Lerp(ReboundDegrees,ReboundDegrees+4,Phase/.15f);
            else if (Phase<Contact)
            {
                const float T=(Phase-.15f)/(Contact-.15f);
                Lift=(ReboundDegrees+4)*(1-T*T);
            }
            else if (Phase<Contact+.08f) Lift=0;
            else Lift=ReboundDegrees*FMath::SmoothStep(Contact+.08f,1.f,Phase);
        }
        const FVector ArcRight=FVector::CrossProduct(Up,WorkingPoint-Elbow).GetSafeNormal(SMALL_NUMBER,Right);
        const FQuat Recoil(ArcRight,FMath::DegreesToRadians(-Lift));
        const FVector WorkingDirection=Recoil.RotateVector(ContactGrip-Elbow).GetSafeNormal();
        FQuat WorkingRotation=Recoil*ContactRotation;
        float Blend=bSwingActive ? (bFirstStroke?FMath::SmoothStep(0.f,.22f,Phase):1.f) : 1-FMath::SmoothStep(0.f,1.f,Release);
        // Blend the whole arm into the working arc; the pose solver preserves both limb lengths.
        const FVector IdleDirection=(IdleGrip-Elbow).GetSafeNormal();
        const FQuat DirectionBlend=FQuat::Slerp(FQuat::Identity,FQuat::FindBetweenNormals(IdleDirection,WorkingDirection),Blend);
        Grip=FMath::Lerp(IdleGrip, Elbow+DirectionBlend.RotateVector(IdleDirection)*(ContactGrip-Elbow).Size(), Blend);
        Rotation=FQuat::Slerp(IdleRotation,WorkingRotation,Blend);
        Load=Blend*TorsoLean;
    }
    KeepToolOutsideBody(Grip,Rotation);
    Tip=Grip+Rotation.RotateVector(TipOffset);
    return true;
}
void UJTSMiningStrikeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bSwingActive || bContactResolved || !GetToolDefinition()) { SetComponentTickEnabled(false); return; }
	ResolveContact();
}

void UJTSMiningStrikeComponent::ResolveContact()
{
	const auto* Item = GetToolDefinition();
	if (!GetOwner()->HasAuthority() || !Item || !bSwingActive || bContactResolved) return;
	const float Elapsed = GetWorld()->GetTimeSeconds() - SwingStart;
	const float ContactTime = Item->MiningSwingDuration * Item->MiningContactFraction;
	if (Elapsed + KINDA_SMALL_NUMBER < ContactTime) return;
	// A hitch crossing the whole window still gets one authoritative sample. Never reopen it later.
	const bool bCrossedWindow = LastContactSample < ContactTime;
	LastContactSample = Elapsed;
	if (Elapsed > ContactTime + ContactWindowSeconds && !bCrossedWindow) { bContactResolved = true; return; }
	auto* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn) return;
	FHitResult Hit;
	const bool bFound = ExplicitWorkTarget.IsValid() ? FindWorkContact(ExplicitWorkTarget.Get(), Hit) : AcquireContact(Hit);
	if (!bFound) return;
	AActor* Target = Hit.GetActor();
	if (!IsValid(Target)) return;
	bool bAccepted = false;
	if (Target == ExplicitWorkTarget.Get())
	{
		if (auto* Work = Target->FindComponentByClass<UJTSNestBlockadeComponent>()) bAccepted = Work->ApplyStrikeWork(Pawn);
	}
	else if (auto* Melee = Pawn->FindComponentByClass<UJTSMeleeComponent>())
	{
		// Feedback is emitted before damage can destroy the target and close its actor channel.
		bAccepted = Melee->CanApplyToolContact(Target);
		if (bAccepted)
		{
			if (auto* Impact = Target->FindComponentByClass<UJTSMiningImpactComponent>()) Impact->ConfirmImpact(Hit);
			bAccepted = Melee->ApplyToolContact(Target);
		}
	}
	if (bAccepted)
	{
		if (Target == ExplicitWorkTarget.Get())
			if (auto* Impact = Target->FindComponentByClass<UJTSMiningImpactComponent>()) Impact->ConfirmImpact(Hit);
		bContactResolved = true;
		MulticastToolContact(Hit.ImpactPoint);
		++ConfirmedImpactCount; LastImpactLocation = Hit.ImpactPoint;
		ClientConfirmImpact();
	}
}
void UJTSMiningStrikeComponent::MulticastToolContact_Implementation(FVector_NetQuantize Point)
{
	ContactPoint = Point;
	PresentationTarget.Reset();
	bContactResolved = true;
}
void UJTSMiningStrikeComponent::ClientConfirmImpact_Implementation()
{
	LastImpact = GetWorld()->GetTimeSeconds();
	if (auto* Character = Cast<AJTSCharacter>(GetOwner())) Character->ApplyWeaponViewKick(.65f);
}
float UJTSMiningStrikeComponent::GetImpactFeedback() const
{
	return GetWorld() ? FMath::Clamp(1.f-float(GetWorld()->GetTimeSeconds()-LastImpact)/.16f,0.f,1.f) : 0;
}
void UJTSMiningStrikeComponent::CancelSwing()
{
    if (bSwingActive && GetWorld()) ReturnStart=GetWorld()->GetTimeSeconds();
    bSwingActive=false; bContactResolved=true; ExplicitWorkTarget.Reset(); PresentationTarget.Reset(); SetComponentTickEnabled(false);
}
