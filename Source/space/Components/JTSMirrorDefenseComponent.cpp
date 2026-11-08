#include "JTSMirrorDefenseComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "space/Characters/JTSPlanetSurfaceCreature.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSRangedWeaponComponent.h"
#include "space/Systems/JTSMirrorGlareSubsystem.h"

UJTSMirrorDefenseComponent::UJTSMirrorDefenseComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
}

void UJTSMirrorDefenseComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(bEnabled && GetNetMode() != NM_DedicatedServer);
	CreatureMesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
	PreviousLocation = GetOwner()->GetActorLocation();
	if (CreatureMesh.IsValid()) CreatureMesh->AddTickPrerequisiteComponent(this);
	if (bEnabled && GetNetMode() != NM_DedicatedServer)
		GetWorld()->GetSubsystem<UJTSMirrorGlareSubsystem>()->RegisterSource(this);
}

void UJTSMirrorDefenseComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UWorld* World = GetWorld())
		if (auto* System = World->GetSubsystem<UJTSMirrorGlareSubsystem>()) System->UnregisterSource(this);
	Super::EndPlay(Reason);
}

FQuat UJTSMirrorDefenseComponent::CalculateLean(const FVector& LocalDirection, float MaximumDegrees)
{
	const FVector Direction = LocalDirection.GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
	const FQuat Turn = FQuat::FindBetweenNormals(FVector::UpVector, Direction);
	const float Angle = Turn.GetAngle();
	return FQuat::Slerp(FQuat::Identity, Turn, FMath::Min(1.f, FMath::DegreesToRadians(MaximumDegrees) / FMath::Max(Angle, .001f)));
}

void UJTSMirrorDefenseComponent::AimAt(APawn* Target)
{
	if (!bEnabled || !GetOwner()->HasAuthority() || !IsValid(Target)) return;
	FVector Eye = Target->GetPawnViewLocation(), Ray;
	if (const auto* View = Target->FindComponentByClass<UJTSRangedWeaponComponent>()) View->GetCameraRay(Eye, Ray);
	AimDirection = (Eye - GetMirrorLocation()).GetSafeNormal(SMALL_NUMBER, GetOwner()->GetActorUpVector());
	bReflecting = true;
}

void UJTSMirrorDefenseComponent::StopReflection()
{
	if (GetOwner()->HasAuthority()) bReflecting = false;
}

void UJTSMirrorDefenseComponent::TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(Dt, TickType, TickFunction);
	ReflectionBlend = FMath::FInterpConstantTo(ReflectionBlend, bReflecting ? 1.f : 0.f, Dt,
		bReflecting ? 2.2f : 1.f / FMath::Max(.1f, RetractSeconds));
	const FVector Local = GetOwner()->GetActorTransform().InverseTransformVectorNoScale(AimDirection);
	const FQuat Desired = FQuat::Slerp(FQuat::Identity, CalculateLean(Local, MaximumLeanDegrees), ReflectionBlend);
	PresentationLean = FQuat::Slerp(PresentationLean, Desired, 1.f - FMath::Exp(-Dt * 8.f));
	const FVector Location = GetOwner()->GetActorLocation();
	const FVector Travel = FVector::VectorPlaneProject(Location - PreviousLocation, GetOwner()->GetActorUpVector());
	PreviousLocation = Location;
	const bool bWalking = Travel.SizeSquared() > .0001f && Travel.SizeSquared() < FMath::Square(100.f);
	if (bWalking)
	{
		GaitPhase = FMath::Fmod(GaitPhase + Travel.Size() / FMath::Max(1.f, StrideLength) * 2 * PI, 2 * PI);
		GaitDirection = GetOwner()->GetActorTransform().InverseTransformVectorNoScale(Travel).GetSafeNormal();
	}
	GaitBlend = FMath::FInterpTo(GaitBlend, bWalking ? 1.f : 0.f, Dt, 12.f);
}

FVector UJTSMirrorDefenseComponent::GetMirrorLocation() const
{
	if (CreatureMesh.IsValid())
	{
		const int32 Index = CreatureMesh->GetBoneIndex(BodyBone);
		if (Index != INDEX_NONE)
		{
			const FVector Pivot = CreatureMesh->GetBoneTransform(Index).GetLocation();
			return Pivot + GetMirrorNormal() * FMath::Max(1.f, MirrorHeightAboveBody);
		}
	}
	return GetOwner()->GetActorTransform().TransformPosition(MirrorOffset);
}

FVector UJTSMirrorDefenseComponent::GetMirrorNormal() const
{
	return GetOwner()->GetActorQuat().RotateVector(PresentationLean.RotateVector(FVector::UpVector));
}

void UJTSMirrorDefenseComponent::SetNearbyReflection(UTexture* Texture, bool bVisible)
{
	if (!CreatureMesh.IsValid()) return;
	if (bVisible && ReflectiveMaterials.IsEmpty())
		for (int32 Slot : ReflectiveMaterialSlots)
			if (auto* Material = CreatureMesh->CreateDynamicMaterialInstance(Slot)) ReflectiveMaterials.Add(Material);
	for (const auto& Material : ReflectiveMaterials) if (Material)
	{
		if (Texture) Material->SetTextureParameterValue(TEXT("NearbyReflection"), Texture);
		Material->SetScalarParameterValue(TEXT("NearbyReflectionWeight"), bVisible ? 1.f : 0.f);
	}
}

bool UJTSMirrorDefenseComponent::CanGlareAt(APlayerController* Viewer) const
{
	const auto* Creature = Cast<AJTSPlanetSurfaceCreature>(GetOwner());
	if (!bEnabled || !bReflecting || ReflectionBlend < .8f || !Creature || !Creature->IsAlive()
		|| GetOwner()->IsHidden() || !Viewer || !Viewer->GetPawn()) return false;
	if (const auto* Health = Viewer->GetPawn()->FindComponentByClass<UJTSHealthComponent>(); Health && Health->IsDead()) return false;
	FVector Eye; FRotator View;
	Viewer->GetPlayerViewPoint(Eye, View);
	const FVector ToEye = Eye - GetMirrorLocation();
	if (ToEye.SizeSquared() > FMath::Square(GlareRange) || ToEye.IsNearlyZero()) return false;
	const FVector Direction = ToEye.GetSafeNormal();
	if (FVector::DotProduct(GetMirrorNormal(), Direction) < FMath::Cos(FMath::DegreesToRadians(GlareHalfAngle))) return false;
	FVector2D Screen;
	if (!Viewer->ProjectWorldLocationToScreen(GetMirrorLocation(), Screen, true)) return false;
	int32 Width, Height; Viewer->GetViewportSize(Width, Height);
	if (Screen.X < 0 || Screen.Y < 0 || Screen.X > Width || Screen.Y > Height) return false;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MirrorGlare), true, GetOwner());
	Params.AddIgnoredActor(Viewer->GetPawn());
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, GetMirrorLocation(), Eye, ECC_Visibility, Params);
}

void UJTSMirrorDefenseComponent::ApplyPose(USkeletalMeshComponent* Mesh)
{
	if (!bEnabled || BodyBone.IsNone() || !Mesh->GetSkeletalMeshAsset()) return;
	const auto* Creature = Cast<AJTSPlanetSurfaceCreature>(GetOwner());
	if (!Creature || !Creature->IsAlive()) return;
	TArray<FTransform>& Pose = Mesh->GetEditableComponentSpaceTransforms();
	const int32 Body = Mesh->GetBoneIndex(BodyBone);
	if (!Pose.IsValidIndex(Body)) return;
	const TArray<FTransform> Original = Pose;
	const FVector Pivot = Pose[Body].GetLocation();
	const FVector Normal = PresentationLean.RotateVector(FVector::UpVector);
	// Keep the low edge of the rigid carapace above the ground while the supporting legs fold.
	const float Clearance = FMath::Sqrt(FMath::Square(Normal.X * ShellHalfExtent.X) + FMath::Square(Normal.Y * ShellHalfExtent.Y));
	const float StandingHeight = Pivot.Z - Original[0].GetLocation().Z;
	PoseLift = FVector(0,0,BodyLift + FMath::Max(0.f, Clearance - StandingHeight + ShellHalfExtent.Z));
	auto TransformPoint = [&](FVector P) { return Pivot + PresentationLean.RotateVector(P - Pivot) + PoseLift; };
	const FReferenceSkeleton& Skeleton = Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
	if (GaitSkeleton.Get() != Mesh->GetSkeletalMeshAsset() || RestFeet.Num() != Legs.Num())
	{
		GaitSkeleton = Mesh->GetSkeletalMeshAsset();
		TArray<FTransform> RestPose = Skeleton.GetRefBonePose();
		for (int32 I = 1; I < RestPose.Num(); ++I) RestPose[I] *= RestPose[ Skeleton.GetParentIndex(I) ];
		RestFeet.Reset();
		for (const auto& Leg : Legs)
		{
			const int32 Foot = Mesh->GetBoneIndex(Leg.Foot);
			RestFeet.Add(RestPose.IsValidIndex(Foot) ? RestPose[Foot].GetLocation() : FVector::ZeroVector);
		}
	}
	for (int32 I = 0; I < Pose.Num(); ++I)
	{
		if (I != Body && !Skeleton.BoneIsChildOf(I, Body)) continue;
		Pose[I].SetLocation(TransformPoint(Original[I].GetLocation()));
		Pose[I].SetRotation(PresentationLean * Original[I].GetRotation());
	}
	for (int32 LegIndex = 0; LegIndex < Legs.Num(); ++LegIndex)
	{
		const auto& Leg = Legs[LegIndex];
		const int32 A = Mesh->GetBoneIndex(Leg.Upper), B = Mesh->GetBoneIndex(Leg.Lower), C = Mesh->GetBoneIndex(Leg.Foot);
		if (!Pose.IsValidIndex(A) || !Pose.IsValidIndex(B) || !Pose.IsValidIndex(C)) continue;
		const FVector Hip = TransformPoint(Original[A].GetLocation());
		const FVector OldUpper = Original[B].GetLocation() - Original[A].GetLocation();
		const FVector OldLower = Original[C].GetLocation() - Original[B].GetLocation();
		const float UpperLength = OldUpper.Size() * LegLengthScale, LowerLength = OldLower.Size() * LegLengthScale;
		// Opposite tripods alternate; phase follows actual surface travel rather than the AI timer.
		const float Phase = GaitPhase + ((LegIndex % 3 + LegIndex / 3) % 2) * PI;
		const float Weight = GaitBlend * (1.f - ReflectionBlend);
		const FVector Step = (GaitDirection * (-FMath::Cos(Phase) * StrideLength * .25f)
			+ FVector::UpVector * (FMath::Max(0.f, FMath::Sin(Phase)) * StepHeight)) * Weight;
		// Replace the baked foot cycle while travelling so two unrelated gait phases cannot fight each other.
		const FVector FootBase = FMath::Lerp(Original[C].GetLocation(), RestFeet[LegIndex], Weight);
		const FVector Delta = FootBase + Step - Hip;
		const float Distance = FMath::Clamp(float(Delta.Size()), FMath::Abs(UpperLength - LowerLength) + .01f, (UpperLength + LowerLength) * .999f);
		const FVector Axis = Delta.GetSafeNormal(SMALL_NUMBER, FVector::DownVector);
		FVector Pole = FVector::VectorPlaneProject(OldUpper, Axis).GetSafeNormal();
		if (Pole.IsNearlyZero()) Pole = FVector::VectorPlaneProject(FVector::UpVector, Axis).GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector);
		const float Along = (UpperLength*UpperLength - LowerLength*LowerLength + Distance*Distance) / (2*Distance);
		const FVector Knee = Hip + Axis*Along + Pole*FMath::Sqrt(FMath::Max(0.f, UpperLength*UpperLength - Along*Along));
		const FVector Foot = Hip + Axis*Distance;
		Pose[A].SetLocation(Hip); Pose[B].SetLocation(Knee); Pose[C].SetLocation(Foot);
		Pose[A].SetRotation(FQuat::FindBetweenNormals(OldUpper.GetSafeNormal(), (Knee-Hip).GetSafeNormal()) * Original[A].GetRotation());
		Pose[B].SetRotation(FQuat::FindBetweenNormals(OldLower.GetSafeNormal(), (Foot-Knee).GetSafeNormal()) * Original[B].GetRotation());
		Pose[A].SetScale3D(Original[A].GetScale3D() * LegLengthScale);
		Pose[B].SetScale3D(Original[B].GetScale3D() * LegLengthScale);
		Pose[C].SetRotation(Original[C].GetRotation());
	}
}

void UJTSMirrorDefenseComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UJTSMirrorDefenseComponent, bReflecting);
	DOREPLIFETIME(UJTSMirrorDefenseComponent, AimDirection);
}

void UJTSReflectorSkeletalMeshComponent::FinalizeBoneTransform()
{
	if (AActor* Actor = GetOwner())
		if (auto* Mirror = Actor->FindComponentByClass<UJTSMirrorDefenseComponent>()) Mirror->ApplyPose(this);
	Super::FinalizeBoneTransform();
}
