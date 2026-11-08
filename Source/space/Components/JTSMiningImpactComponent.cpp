#include "JTSMiningImpactComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"

UJTSMiningImpactComponent::UJTSMiningImpactComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}
UStaticMeshComponent* UJTSMiningImpactComponent::GetSurfaceMesh() const
{
	TArray<UStaticMeshComponent*> Meshes; GetOwner()->GetComponents(Meshes);
	for (auto* Mesh : Meshes) if (Mesh->ComponentHasTag(TEXT("MiningSurface"))) return Mesh;
	return nullptr;
}
void UJTSMiningImpactComponent::ConfirmImpact(const FHitResult& Hit, bool bBreaking)
{
	if (!GetOwner()->HasAuthority()) return;
	LastPoint = Hit.ImpactPoint; LastNormal = Hit.ImpactNormal;
	++ImpactCount;
	MulticastImpact(Hit.ImpactPoint, Hit.ImpactNormal, bBreaking);
}
void UJTSMiningImpactComponent::ConfirmBreak()
{
	if (GetOwner()->HasAuthority()) MulticastImpact(LastPoint, LastNormal, true);
}
void UJTSMiningImpactComponent::MulticastImpact_Implementation(FVector_NetQuantize Point, FVector_NetQuantizeNormal Normal, bool bBreaking)
{
	if (GetWorld()->GetNetMode() == NM_DedicatedServer) return;
	USoundBase* Sound = bBreaking ? BreakSound.Get() : !ImpactSounds.IsEmpty() ? ImpactSounds[FMath::RandHelper(ImpactSounds.Num())].Get() : nullptr;
	if (Sound) UGameplayStatics::PlaySoundAtLocation(this,Sound,Point,FRotator::ZeroRotator,
		bBreaking ? 1.3f : 1.15f,FMath::FRandRange(.93f,1.07f),0,ImpactAttenuation);
	if (ImpactDust)
	{
		if (auto* Dust = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(),ImpactDust,Point,
			FRotationMatrix::MakeFromZ(Normal).Rotator(),FVector(DustScale*(bBreaking?2.2f:1.f))))
		{
			Dust->SetVariableLinearColor(TEXT("User.Color"),DustColor);
			Dust->SetVariableLinearColor(TEXT("User.DustColor"),DustColor);
		}
	}
	if (!bBreaking && ScarMaterial)
	{
		if (Scars.Num() >= 6) { if (IsValid(Scars[0])) Scars[0]->DestroyComponent(); Scars.RemoveAt(0); }
		if (auto* Scar = UGameplayStatics::SpawnDecalAttached(ScarMaterial,FVector(3,6,6),GetSurfaceMesh(),NAME_None,
			Point+Normal*.6f,Normal.Rotation(),EAttachLocation::KeepWorldPosition,10.f))
		{
			Scar->SetFadeOut(7.f,3.f,false); Scars.Add(Scar);
		}
	}
	const FVector Up = GetSurfaceMesh() ? GetSurfaceMesh()->GetUpVector() : GetOwner()->GetActorUpVector();
	const FVector Side = FVector::CrossProduct(Normal,Up).GetSafeNormal(SMALL_NUMBER,FVector::RightVector);
	// Short emissive streaks originate at the validated surface contact, never at the pawn.
	for (int32 Index=0; SparkMesh && SparkMaterial && Index<SparkCount && Fragments.Num()<72; ++Index)
	{
		auto* Mesh=NewObject<UStaticMeshComponent>(GetOwner());
		Mesh->SetStaticMesh(SparkMesh); Mesh->SetMaterial(0,SparkMaterial);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetCastShadow(false); Mesh->RegisterComponentWithWorld(GetWorld());
		const float Extent=FMath::Max(1.f,SparkMesh->GetBoundingBox().GetSize().GetMax());
		Mesh->SetWorldScale3D(FVector(FMath::FRandRange(5.f,12.f),.8f,.8f)/Extent);
		FFragment Spark; Spark.Mesh=Mesh; Spark.Start=Point+Normal*3; Spark.Born=GetWorld()->GetTimeSeconds();
		Spark.bSpark=true; Spark.Lifetime=FMath::FRandRange(.13f,.23f);
		Spark.Velocity=FVector(Normal)*FMath::FRandRange(90.f,180.f)+Up*FMath::FRandRange(50.f,150.f)+Side*FMath::FRandRange(-150.f,150.f);
		Mesh->SetWorldLocation(Spark.Start); Mesh->SetWorldRotation(Spark.Velocity.Rotation()); Fragments.Add(Spark);
	}
	const int32 Count = bBreaking ? 12 : 6;
	for (int32 Index=0; FragmentMesh && Index<Count && Fragments.Num()<72; ++Index)
	{
		auto* Mesh = NewObject<UStaticMeshComponent>(GetOwner());
		Mesh->SetStaticMesh(FragmentMesh);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetCastShadow(false);
		Mesh->RegisterComponentWithWorld(GetWorld());
		const float Size=FMath::FRandRange(2.f,5.f);
		const float Extent=FMath::Max(1.f,FragmentMesh->GetBoundingBox().GetSize().GetMax());
		Mesh->SetWorldScale3D(FVector(Size/Extent));
		Mesh->SetWorldLocation(Point+Normal*2);
		FFragment Fragment;
		Fragment.Mesh=Mesh; Fragment.Start=Point+Normal*2; Fragment.Born=GetWorld()->GetTimeSeconds();
		Fragment.Velocity=FVector(Normal)*FMath::FRandRange(50.f,115.f)+Up*FMath::FRandRange(35.f,95.f)+Side*FMath::FRandRange(-70.f,70.f);
		Fragment.Spin=FRotator(FMath::FRandRange(-600.f,600.f),FMath::FRandRange(-600.f,600.f),FMath::FRandRange(-600.f,600.f));
		Fragments.Add(Fragment);
	}
	SetComponentTickEnabled(true);
}
void UJTSMiningImpactComponent::TickComponent(float Dt, ELevelTick Type, FActorComponentTickFunction* Function)
{
	Super::TickComponent(Dt,Type,Function);
	const FVector Up=GetSurfaceMesh()?GetSurfaceMesh()->GetUpVector():GetOwner()->GetActorUpVector();
	for (int32 Index=Fragments.Num()-1; Index>=0; --Index)
	{
		auto& Fragment=Fragments[Index];
		const float Age=float(GetWorld()->GetTimeSeconds()-Fragment.Born);
		if (!Fragment.Mesh.IsValid() || Age>Fragment.Lifetime)
		{
			if (Fragment.Mesh.IsValid()) Fragment.Mesh->DestroyComponent();
			Fragments.RemoveAtSwap(Index); continue;
		}
		Fragment.Mesh->SetWorldLocation(Fragment.Start+Fragment.Velocity*Age-Up*(240*Age*Age));
		if (!Fragment.bSpark) Fragment.Mesh->AddWorldRotation(Fragment.Spin*Dt);
		if (Age>Fragment.Lifetime*.6f) Fragment.Mesh->SetWorldScale3D(Fragment.Mesh->GetComponentScale()*FMath::Max(0.f,1-Dt*14));
	}
	if (Fragments.IsEmpty()) SetComponentTickEnabled(false);
}
void UJTSMiningImpactComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	for (auto& Fragment:Fragments) if (Fragment.Mesh.IsValid()) Fragment.Mesh->DestroyComponent();
	for (const auto& Scar:Scars) if (IsValid(Scar)) Scar->DestroyComponent();
	Super::EndPlay(Reason);
}
