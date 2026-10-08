#include "space/Weapons/JTSStellarStatusEffectActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/RotationMatrix.h"

AJTSStellarStatusEffectActor::AJTSStellarStatusEffectActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.f / 30.f;
	SetActorEnableCollision(false);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	IceShards = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("IceShards"));
	Flames = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Flames"));
	Embers = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Embers"));
	for (UStaticMeshComponent* Mesh : { IceShards.Get(), Flames.Get(), Embers.Get() })
	{
		Mesh->SetupAttachment(GetRootComponent());
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCastShadow(false);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetVisibility(false);
	}
}

void AJTSStellarStatusEffectActor::SetLayer(UStaticMeshComponent* Mesh, bool Visible, FLinearColor Color)
{
	Mesh->SetVisibility(Visible);
	if (!Visible || !Mesh->GetMaterial(0)) return;
	auto* Material = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
	if (!Material) Material = Mesh->CreateAndSetMaterialInstanceDynamic(0);
	Material->SetVectorParameterValue(TEXT("Color"), Color);
}

void AJTSStellarStatusEffectActor::UpdateStatus(bool Frozen, bool Burning, bool LightBurning, bool Poisoned)
{
	bFrozen = Frozen; bBurning = Burning; bLightBurning = LightBurning; bPoisoned = Poisoned;
	SetLayer(IceShards, Frozen, IceColor);
	SetLayer(Flames, Burning || LightBurning, Burning ? FireColor : LightFireColor);
	SetLayer(Embers, Burning || LightBurning, Burning ? FireColor : LightFireColor);
	Tick(0);
}

void AJTSStellarStatusEffectActor::FindBodyMesh()
{
	if (BodyMesh.IsValid() || !GetOwner()) return;
	TInlineComponentArray<UMeshComponent*> Meshes(GetOwner());
	double Largest = 0;
	for (auto* Mesh : Meshes)
	{
		if (!Mesh->IsVisible() || (!Cast<UStaticMeshComponent>(Mesh) && !Cast<USkeletalMeshComponent>(Mesh))) continue;
		const FVector Size = Mesh->Bounds.BoxExtent;
		const double Volume = Size.X * Size.Y * Size.Z;
		if (Volume > Largest) { Largest = Volume; BodyMesh = Mesh; }
	}
	if (BodyMesh.IsValid() && BodyTintMaterial)
	{
		OriginalOverlay = BodyMesh->GetOverlayMaterial();
		BodyTint = UMaterialInstanceDynamic::Create(BodyTintMaterial, this);
		BodyMesh->SetOverlayMaterial(BodyTint);
	}
}

void AJTSStellarStatusEffectActor::UpdateInstances(UInstancedStaticMeshComponent* Mesh, const TArray<FTransform>& Transforms)
{
	if (Mesh->GetInstanceCount() != Transforms.Num())
	{
		Mesh->ClearInstances(); Mesh->AddInstances(Transforms, false, true);
	}
	else Mesh->BatchUpdateInstancesTransforms(0, Transforms, true, true, true);
}

void AJTSStellarStatusEffectActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AActor* Target = GetOwner();
	if (!IsValid(Target)) { Destroy(); return; }
	FindBodyMesh();
	FTransform Frame = Target->GetActorTransform();
	FVector Center = FVector::ZeroVector, Extent(35);
	if (BodyMesh.IsValid())
	{
		Frame = BodyMesh->GetComponentTransform();
		const FBoxSphereBounds Bounds = BodyMesh->CalcBounds(FTransform::Identity);
		Center = Bounds.Origin; Extent = Bounds.BoxExtent.ComponentMax(FVector(5));
	}
	else
	{
		FVector WorldCenter, WorldExtent; Target->GetActorBounds(true, WorldCenter, WorldExtent);
		Center = Frame.InverseTransformPosition(WorldCenter);
		Extent = WorldExtent.ComponentMax(FVector(20));
	}
	const FVector Up = Target->GetActorUpVector();
	const float Radius = FMath::Clamp(static_cast<float>(FMath::Max(Extent.X, Extent.Y)), 15.f, 150.f);
	const float Time = GetWorld()->GetTimeSeconds();
	SetActorLocation(Frame.TransformPosition(Center));
	if (BodyTint)
	{
		BodyTint->SetScalarParameterValue(TEXT("Frozen"), bFrozen);
		BodyTint->SetScalarParameterValue(TEXT("Burning"), bBurning || bLightBurning);
		BodyTint->SetScalarParameterValue(TEXT("Poisoned"), bPoisoned);
		BodyTint->SetVectorParameterValue(TEXT("IceColor"), IceColor);
		BodyTint->SetVectorParameterValue(TEXT("FireColor"), bBurning ? FireColor : LightFireColor);
		BodyTint->SetVectorParameterValue(TEXT("PoisonColor"), PoisonColor);
		BodyTint->SetVectorParameterValue(TEXT("BodyCenter"), FLinearColor(Frame.TransformPosition(Center)));
		BodyTint->SetVectorParameterValue(TEXT("BodyX"), FLinearColor(Frame.GetUnitAxis(EAxis::X)));
		BodyTint->SetVectorParameterValue(TEXT("BodyY"), FLinearColor(Frame.GetUnitAxis(EAxis::Y)));
		BodyTint->SetVectorParameterValue(TEXT("BodyZ"), FLinearColor(Frame.GetUnitAxis(EAxis::Z)));
		BodyTint->SetVectorParameterValue(TEXT("BodyExtent"), FLinearColor(Extent * Frame.GetScale3D().GetAbs()));
	}
	if (bFrozen)
	{
		TArray<FTransform> Shards;
		for (int32 I = 0; I < 8; ++I)
		{
			const FVector Corner(I & 1 ? 1 : -1, I & 2 ? 1 : -1, I & 4 ? .78f : -.78f);
			const FVector Local = Center + Corner * Extent * .92f;
			FVector Position = Frame.TransformPosition(Local);
			FVector Outward = Frame.TransformVectorNoScale(Corner).GetSafeNormal();
			if (IceSockets.IsValidIndex(I) && BodyMesh.IsValid() && BodyMesh->DoesSocketExist(IceSockets[I]))
			{
				Position = BodyMesh->GetSocketLocation(IceSockets[I]);
				Outward = (Position - Frame.TransformPosition(Center)).GetSafeNormal();
			}
			const float Size = Radius * Frame.GetScale3D().GetAbsMax();
			Shards.Emplace(FRotationMatrix::MakeFromZ(Outward).ToQuat(), Position + Outward * Size * .16f,
				FVector(Size * .32f / 50, Size * .32f / 50, Size * .85f / 100));
		}
		UpdateInstances(IceShards, Shards);
	}
	if (bBurning || bLightBurning)
	{
		// A compact flame crown keeps the torso and frozen extremities readable.
		FVector Head = Frame.TransformPosition(Center + FVector(0, 0, Extent.Z));
		if (!HeadSocket.IsNone() && BodyMesh.IsValid() && BodyMesh->DoesSocketExist(HeadSocket)) Head = BodyMesh->GetSocketLocation(HeadSocket);
		const float Size = Radius * Frame.GetScale3D().GetAbsMax();
		const FQuat Rotation = FRotationMatrix::MakeFromZX(Up, Target->GetActorForwardVector()).ToQuat();
		TArray<FTransform> Fire, Sparks;
		for (int32 I = 0; I < 5; ++I)
		{
			const float Phase = FMath::Frac(Time * (1.5f + .09f * I) + I * .193f);
			const float Angle = I * 2.4f;
			const float Length = Size * (1.05f + .65f * FMath::Sin(Phase * PI));
			const FVector Side = Rotation.RotateVector(FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0));
			const FVector Position = Head + Side * Size * .28f + Up * Length * .4f;
			const FVector Lean = (Up + Side * (.1f + .1f * FMath::Sin(Time * 5 + I))).GetSafeNormal();
			Fire.Emplace(FRotationMatrix::MakeFromZ(Lean).ToQuat(), Position, FVector(Size * .38f / 50, Size * .38f / 50, Length / 100));
			const FVector Spark = Head + Side * Size * (.25f + Phase * .55f) + Up * Size * (1.f + Phase * 2);
			Sparks.Emplace(FQuat::Identity, Spark, FVector((2 + (1 - Phase) * 2) / 50));
		}
		UpdateInstances(Flames, Fire); UpdateInstances(Embers, Sparks);
	}
}

void AJTSStellarStatusEffectActor::EndPlay(const EEndPlayReason::Type Reason)
{
	if (BodyMesh.IsValid() && BodyTint && BodyMesh->GetOverlayMaterial() == BodyTint) BodyMesh->SetOverlayMaterial(OriginalOverlay);
	Super::EndPlay(Reason);
}
