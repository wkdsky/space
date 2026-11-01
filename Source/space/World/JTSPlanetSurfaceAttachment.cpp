#include "space/World/JTSPlanetSurfaceAttachment.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/StaticMesh.h"
#include "KismetProceduralMeshLibrary.h"
#include "ProceduralMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Components/JTSPlanetSurfaceCutComponent.h"

AJTSPlanetSurfaceAttachment::AJTSPlanetSurfaceAttachment()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("AttachmentRoot")));
	SolidMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SolidAttachment"));
	SolidMesh->SetupAttachment(GetRootComponent());
	SolidMesh->SetCollisionProfileName(TEXT("NoCollision"));
	SolidMesh->SetCanEverAffectNavigation(false);
	SurfacePatch = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ConformingAttachment"));
	SurfacePatch->SetupAttachment(GetRootComponent());
	SurfacePatch->SetCollisionProfileName(TEXT("NoCollision"));
	SurfacePatch->SetCanEverAffectNavigation(false);
	Health = CreateDefaultSubobject<UJTSHealthComponent>(TEXT("Health"));
	DamageTarget = CreateDefaultSubobject<UBoxComponent>(TEXT("DamageTarget"));
	DamageTarget->SetupAttachment(GetRootComponent());
	DamageTarget->SetCollisionObjectType(ECC_WorldDynamic);
	DamageTarget->SetCollisionResponseToAllChannels(ECR_Ignore);
	DamageTarget->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	DamageTarget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DamageTarget->SetGenerateOverlapEvents(false);
	DamageTarget->SetCanEverAffectNavigation(false);
}
void AJTSPlanetSurfaceAttachment::Initialize(UStaticMesh* Mesh, AJTSPlanetAnchor* Planet,
	const FTransform& Transform, bool bConform, bool bCastShadow, float SurfaceOffset, bool bDestructible, UStaticMesh* CutVolume)
{
	if (!HasAuthority()) return;
	Appearance.Mesh = Mesh; Appearance.Planet = Planet; Appearance.Transform = Transform;
	Appearance.bConform = bConform; Appearance.bCastShadow = bCastShadow; Appearance.SurfaceOffset = SurfaceOffset;
	Appearance.bDestructible = bDestructible;
	Appearance.CutVolume = CutVolume;
	bAlwaysRelevant = CutVolume != nullptr;
	SetOwner(Planet); Tags.AddUnique(TEXT("PlanetSurfaceContent")); ApplyAppearance();
}
void AJTSPlanetSurfaceAttachment::BeginPlay()
{
	Super::BeginPlay();
	GetRootComponent()->TransformUpdated.AddUObject(this, &ThisClass::OnAttachmentTransformUpdated);
	if (HasAuthority()) Health->OnDeath.AddDynamic(this, &ThisClass::OnAttachmentDepleted);
	ApplyAppearance();
}
void AJTSPlanetSurfaceAttachment::EndPlay(const EEndPlayReason::Type Reason)
{
	if (IsValid(Appearance.Planet) && Appearance.Planet->SurfaceCuts)
	{
		Appearance.Planet->SurfaceCuts->OnCutsRebuilt.RemoveAll(this);
		Appearance.Planet->SurfaceCuts->RemoveCut(this);
	}
	Super::EndPlay(Reason);
}
bool AJTSPlanetSurfaceAttachment::IsRefugeUsable() const
{
	return !Health->IsDead() && (!Appearance.CutVolume || (IsValid(Appearance.Planet) && Appearance.Planet->SurfaceCuts->IsCutApplied(this)));
}
void AJTSPlanetSurfaceAttachment::UpdateCutVisibility()
{
	const bool bUsable = IsRefugeUsable();
	SolidMesh->SetVisibility(bUsable && !Appearance.bConform);
	SurfacePatch->SetVisibility(bUsable && Appearance.bConform);
	DamageTarget->SetCollisionEnabled(bUsable && Appearance.bDestructible ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}
void AJTSPlanetSurfaceAttachment::OnAttachmentTransformUpdated(USceneComponent* Component, EUpdateTransformFlags Flags, ETeleportType Teleport)
{
	if (!bApplyingAppearance && Appearance.CutVolume && IsValid(Appearance.Planet))
	{
		Appearance.Transform = GetActorTransform();
		ApplyAppearance();
	}
}
void AJTSPlanetSurfaceAttachment::ApplyAppearance()
{
	if (!Appearance.Mesh || !Appearance.Planet) return;
	TGuardValue<bool> Applying(bApplyingAppearance, true);
	SetActorTransform(Appearance.Transform);
	SolidMesh->SetCastShadow(Appearance.bCastShadow); SurfacePatch->SetCastShadow(Appearance.bCastShadow);
	SolidMesh->SetStaticMesh(Appearance.Mesh);
	SolidMesh->SetVisibility(!Appearance.bConform); SurfacePatch->SetVisibility(Appearance.bConform);
	SurfacePatch->ClearAllMeshSections();
	SetCanBeDamaged(Appearance.bDestructible);
	const FBoxSphereBounds Bounds = Appearance.Mesh->GetBounds();
	DamageTarget->SetRelativeLocation(Bounds.Origin);
	DamageTarget->SetBoxExtent(Bounds.BoxExtent.ComponentMax(FVector(10)));
	DamageTarget->SetCollisionEnabled(Appearance.bDestructible ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	if (Appearance.CutVolume)
	{
		Appearance.Planet->SurfaceCuts->OnCutsRebuilt.RemoveAll(this);
		Appearance.Planet->SurfaceCuts->OnCutsRebuilt.AddUObject(this, &ThisClass::UpdateCutVisibility);
		Appearance.Planet->SurfaceCuts->RegisterCut(this, Appearance.CutVolume, GetActorTransform());
	}
	UpdateCutVisibility();
	if (!Appearance.bConform || GetNetMode() == NM_DedicatedServer) return;
	FJTSPlanetSurfaceHit Anchor; double Lift = 0;
	if (Appearance.CutVolume && Appearance.Planet->SurfaceCuts->ProjectOriginalSurface(GetActorLocation(), Anchor))
		Lift = FVector::DotProduct(GetActorLocation() - Anchor.ImpactPoint, Anchor.ImpactNormal);
	// Project a private patch copy. Neither the reusable source asset nor the planet is modified.
	for (int32 Section = 0; Section < Appearance.Mesh->GetNumSections(0); ++Section)
	{
		TArray<FVector> Vertices, Normals; TArray<int32> Triangles;
		TArray<FVector2D> UVs; TArray<FProcMeshTangent> Tangents;
		UKismetProceduralMeshLibrary::GetSectionFromStaticMesh(Appearance.Mesh, 0, Section, Vertices, Triangles, Normals, UVs, Tangents);
		bool bValid = !Vertices.IsEmpty();
		for (int32 I = 0; I < Vertices.Num(); ++I)
		{
			FJTSPlanetSurfaceHit Hit;
			const float Height = Vertices[I].Z * GetActorScale3D().Z;
			const FVector Footprint(Vertices[I].X, Vertices[I].Y, 0);
			const FVector Point = GetActorTransform().TransformPosition(Footprint);
			if (!Appearance.Planet->SurfaceCuts->ProjectOriginalSurface(Point, Hit) && !Appearance.Planet->ProjectPointToSurface(Point, Hit)) { bValid = false; break; }
			Vertices[I] = GetActorTransform().InverseTransformPosition(Hit.ImpactPoint + Hit.ImpactNormal * (Height + Appearance.SurfaceOffset + Lift));
		}
		if (!bValid) continue;
		UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices, Triangles, UVs, Normals, Tangents);
		SurfacePatch->CreateMeshSection_LinearColor(Section, Vertices, Triangles, Normals, UVs, TArray<FLinearColor>(), Tangents, false);
		SurfacePatch->SetMaterial(Section, Appearance.Mesh->GetMaterial(Section));
	}
}

bool AJTSPlanetSurfaceAttachment::CanReceiveMeleeHit_Implementation(APawn* Pawn) const
{
	return Appearance.bDestructible && !Health->IsDead();
}

void AJTSPlanetSurfaceAttachment::ReceiveMeleeHit_Implementation(APawn* Pawn, EJTSMeleeAttackType Type)
{
	if (HasAuthority() && CanReceiveMeleeHit_Implementation(Pawn))
		Health->ApplyDamage(Type == EJTSMeleeAttackType::Punch ? 8 : 20, Pawn ? Pawn->GetController() : nullptr, Pawn);
}

FText AJTSPlanetSurfaceAttachment::GetMeleeTargetDisplayName_Implementation() const { return FText::FromString(TEXT("地表构件")); }
FText AJTSPlanetSurfaceAttachment::GetMeleeTargetPrompt_Implementation(APawn* Pawn) const { return FText::FromString(TEXT("摧毁")); }
FVector AJTSPlanetSurfaceAttachment::GetMeleeTargetAnchorWorldLocation_Implementation() const { return DamageTarget->GetComponentLocation(); }

void AJTSPlanetSurfaceAttachment::OnAttachmentDepleted(AController* InstigatorController, AActor* Causer)
{
	if (HasAuthority() && Appearance.bDestructible) Destroy();
}
void AJTSPlanetSurfaceAttachment::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AJTSPlanetSurfaceAttachment, Appearance, COND_InitialOnly);
}
