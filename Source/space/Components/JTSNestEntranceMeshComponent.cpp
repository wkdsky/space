#include "space/Components/JTSNestEntranceMeshComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UJTSNestEntranceMeshComponent::UJTSNestEntranceMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	bUseAsyncCooking = false;
	bUseComplexAsSimpleCollision = true;
	SetCollisionObjectType(ECC_WorldDynamic);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	// The dark wall overlay represents a hole and must not cast a solid-object shadow.
	SetCastShadow(false);
}

void UJTSNestEntranceMeshComponent::RandomizeShapeWithSeed(int32 InSeed, EJTSNestEntranceProfile Profile)
{
	if (GetWorld() && GetWorld()->IsGameWorld() && GetOwner() && !GetOwner()->HasAuthority()) return;
#if WITH_EDITOR
	Modify();
	if (GetOwner()) GetOwner()->Modify();
#endif
	FRandomStream Random(InSeed);
	Shape.Profile = Profile;
	Shape.Seed = InSeed;
	Shape.Lean = Random.FRandRange(-0.18f, 0.18f);
	Shape.Irregularity = Random.FRandRange(0.08f, 0.22f);
	Shape.FloorRoundness = 0.0f;
	switch (Profile)
	{
	case EJTSNestEntranceProfile::Olive:
		Shape.Width = Random.FRandRange(85.0f, 110.0f);
		Shape.Height = Random.FRandRange(25.0f, 36.0f);
		Shape.ProfilePower = Random.FRandRange(1.8f, 2.6f);
		Shape.FloorRoundness = Random.FRandRange(0.85f, 1.0f);
		break;
	case EJTSNestEntranceProfile::Slit:
		Shape.Width = Random.FRandRange(90.0f, 125.0f);
		Shape.Height = Random.FRandRange(6.0f, 12.0f);
		Shape.ProfilePower = Random.FRandRange(1.7f, 2.7f);
		Shape.FloorRoundness = Random.FRandRange(0.55f, 0.9f);
		break;
	case EJTSNestEntranceProfile::Leaning:
		Shape.Width = Random.FRandRange(48.0f, 68.0f);
		Shape.Height = Random.FRandRange(43.0f, 62.0f);
		Shape.ProfilePower = Random.FRandRange(0.8f, 1.5f);
		Shape.Lean = Random.FRandRange(0.4f, 0.7f) * (Random.RandRange(0, 1) ? 1.0f : -1.0f);
		break;
	default:
		Shape.Width = Random.FRandRange(58.0f, 88.0f);
		Shape.Height = Random.FRandRange(35.0f, 55.0f);
		Shape.ProfilePower = Random.FRandRange(0.8f, 1.4f);
		break;
	}
	bEnabled = true;
	RebuildEntrance();
	if (GetOwner() && GetOwner()->HasAuthority()) GetOwner()->ForceNetUpdate();
}

void UJTSNestEntranceMeshComponent::RandomizeEntrance()
{
	FRandomStream Random(FMath::Rand());
	const auto Profile = static_cast<EJTSNestEntranceProfile>(Random.RandRange(0, 3));
	RandomizeShapeWithSeed(Random.RandHelper(MAX_int32), Profile);
}

void UJTSNestEntranceMeshComponent::RebuildEntrance()
{
	if (!IsRegistered()) return;
	ClearAllMeshSections();
	SetVisibility(bEnabled);
	SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	if (auto* FallbackMesh = Cast<UStaticMeshComponent>(GetAttachParent()))
	{
		FallbackMesh->SetVisibility(!bEnabled);
		FallbackMesh->SetCollisionEnabled(bEnabled ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
	}
	if (!bEnabled) return;

	constexpr int32 Segments = 16;
	const float HalfWidth = FMath::Clamp(Shape.Width, 10.0f, 200.0f) * 0.5f;
	const float Height = FMath::Clamp(Shape.Height, 3.0f, 150.0f);
	const float Power = FMath::Clamp(Shape.ProfilePower, 0.5f, 3.0f);
	const float Lean = FMath::Clamp(Shape.Lean, -0.8f, 0.8f);
	const float Irregularity = FMath::Clamp(Shape.Irregularity, 0.0f, 0.3f);
	const float Border = FMath::Clamp(RimWidth, 0.5f, 15.0f);
	const float CenterHeight = Height * 0.5f * FMath::Clamp(Shape.FloorRoundness, 0.0f, 1.0f);
	const bool bRoundedFloor = CenterHeight > KINDA_SMALL_NUMBER;
	const FVector WallNormal = FVector(1.0f, 0.0f, WallSlope).GetSafeNormal();
	FRandomStream Random(Shape.Seed);
	TArray<float> ContourNoise;
	const int32 ContourPointCount = bRoundedFloor ? Segments * 2 : Segments + 1;
	for (int32 Index = 0; Index < ContourPointCount; ++Index)
		ContourNoise.Add(Random.FRandRange(-Irregularity, Irregularity));
	TArray<FVector> Mouth, Lip, Outer;
	auto AddContourPoint = [&](float Angle, bool bLower, bool bEndpoint)
	{
		const int32 Index = Mouth.Num();
		const int32 Previous = bRoundedFloor ? (Index + ContourPointCount - 1) % ContourPointCount : FMath::Max(0, Index - 1);
		const int32 Next = bRoundedFloor ? (Index + 1) % ContourPointCount : FMath::Min(ContourPointCount - 1, Index + 1);
		const float RadialVariation = 1.0f + ContourNoise[Index] * 0.5f
			+ (ContourNoise[Previous] + ContourNoise[Next]) * 0.25f;
		const float Sine = bEndpoint ? 0.0f : FMath::Sin(Angle);
		const float Z = bLower
			? CenterHeight * (1.0f - FMath::Pow(Sine, Power))
			: CenterHeight + (Height - CenterHeight) * FMath::Pow(Sine, Power) * RadialVariation;
		const float Y = HalfWidth * FMath::Cos(Angle) * RadialVariation + Lean * Z;
		const float OuterZ = FMath::Max(0.0f, Z + (bLower ? -Border : Border) * Sine);
		const float OuterY = Y + Border * FMath::Cos(Angle) + Lean * (OuterZ - Z);
		Mouth.Add(FVector(-WallSlope * Z, Y, Z) + WallNormal * 0.25f);
		Lip.Add(FVector(-WallSlope * Z, Y, Z) + WallNormal * 2.5f);
		Outer.Add(FVector(-WallSlope * OuterZ, OuterY, OuterZ) + WallNormal * 0.1f);
	};
	for (int32 Index = 0; Index <= Segments; ++Index)
		AddContourPoint(PI * Index / Segments, false, Index == 0 || Index == Segments);
	if (bRoundedFloor)
	{
		for (int32 Index = 1; Index < Segments; ++Index)
			AddContourPoint(PI * (Segments - Index) / Segments, true, false);
	}
	const int32 ContourSegments = bRoundedFloor ? Mouth.Num() : Segments;

	TArray<FVector> Vertices, Normals;
	TArray<int32> Triangles;
	TArray<FVector2D> UVs;
	auto AddTriangle = [&](FVector A, FVector B, FVector C, const FVector& Facing)
	{
		FVector Normal = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
		if (FVector::DotProduct(Normal, Facing) < 0.0f)
		{
			Swap(B, C);
			Normal = -Normal;
		}
		const int32 Start = Vertices.Num();
		for (const FVector& Vertex : {A, B, C})
		{
			Vertices.Add(Vertex);
			Normals.Add(Normal);
			UVs.Add(FVector2D(Vertex.Y / 100.0f, Vertex.Z / 100.0f));
		}
		Triangles.Append({Start, Start + 1, Start + 2});
	};
	const FVector Center(-WallSlope * CenterHeight, Lean * CenterHeight, CenterHeight);
	for (int32 Index = 0; Index < ContourSegments; ++Index)
		AddTriangle(Center + WallNormal * 0.25f, Mouth[Index], Mouth[(Index + 1) % Mouth.Num()], WallNormal);
	// Rounded mouths already touch the floor at their lowest point. Only open arches need a threshold.
	if (!bRoundedFloor)
	{
		const float LeftY = Mouth.Last().Y, RightY = Mouth[0].Y;
		const FVector Left(-2.0f, LeftY, 0.25f), Right(-2.0f, RightY, 0.25f);
		const FVector FrontLeft(10.0f, LeftY * 0.9f, 0.25f), FrontRight(10.0f, RightY * 0.9f, 0.25f);
		AddTriangle(Left, FrontLeft, FrontRight, FVector::UpVector);
		AddTriangle(Left, FrontRight, Right, FVector::UpVector);
	}
	CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), true);
	SetMaterial(0, VoidMaterial);

	Vertices.Reset(); Triangles.Reset(); Normals.Reset(); UVs.Reset();
	for (int32 Index = 0; Index < ContourSegments; ++Index)
	{
		const int32 Next = (Index + 1) % Mouth.Num();
		// Leave the floor contact open instead of putting a rock crossbar through a narrow fissure.
		if (bRoundedFloor && Index >= Segments && FMath::Min(Mouth[Index].Z, Mouth[Next].Z) < Border) continue;
		AddTriangle(Lip[Index], Lip[Next], Outer[Next], WallNormal);
		AddTriangle(Lip[Index], Outer[Next], Outer[Index], WallNormal);
		AddTriangle(Mouth[Index], Mouth[Next], Lip[Next], WallNormal);
		AddTriangle(Mouth[Index], Lip[Next], Lip[Index], WallNormal);
	}
	CreateMeshSection_LinearColor(1, Vertices, Triangles, Normals, UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), true);
	SetMaterial(1, RimMaterial);
}

void UJTSNestEntranceMeshComponent::OnRegister()
{
	Super::OnRegister();
	RebuildEntrance();
}

void UJTSNestEntranceMeshComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UJTSNestEntranceMeshComponent, bEnabled);
	DOREPLIFETIME(UJTSNestEntranceMeshComponent, Shape);
	DOREPLIFETIME(UJTSNestEntranceMeshComponent, WallSlope);
	DOREPLIFETIME(UJTSNestEntranceMeshComponent, RimWidth);
}

#if WITH_EDITOR
void UJTSNestEntranceMeshComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RebuildEntrance();
}

void UJTSNestEntranceMeshComponent::PostEditUndo()
{
	Super::PostEditUndo();
	RebuildEntrance();
}
#endif
