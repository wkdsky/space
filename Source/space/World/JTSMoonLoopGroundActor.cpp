#include "JTSMoonLoopGroundActor.h"

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "UObject/UnrealType.h"
#include "space/Modes/JTSMoonGameMode.h"
#include "space/Systems/JTSMoonWrapSubsystem.h"
#include "space/World/JTSMoonWorldActor.h"

namespace
{
	constexpr int32 TileRingDimension = 3;
	constexpr int32 TileRingCount = TileRingDimension * TileRingDimension;
	constexpr float GroundSurfaceZ = 0.0f;
	constexpr float TileTransformTolerance = 0.1f;
	const FIntPoint InvalidTileCoordinate(MAX_int32, MAX_int32);
}

AJTSMoonLoopGroundActor::AJTSMoonLoopGroundActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	VisualTiles.Reserve(TileRingCount);
	PhysicalTiles.Reserve(TileRingCount);
	AssignedTileCoordinates.Init(InvalidTileCoordinate, TileRingCount);

	for (int32 TileIndex = 0; TileIndex < TileRingCount; ++TileIndex)
	{
		const FName VisualName(*FString::Printf(TEXT("VisualTile_%02d"), TileIndex));
		UProceduralMeshComponent* const VisualTile = CreateDefaultSubobject<UProceduralMeshComponent>(VisualName);
		VisualTile->SetupAttachment(SceneRoot);
		VisualTile->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		VisualTile->SetGenerateOverlapEvents(false);
		VisualTile->SetCanEverAffectNavigation(false);
		VisualTile->CastShadow = true;
		VisualTile->SetVisibility(true, true);
		VisualTile->SetHiddenInGame(false);
		VisualTile->SetRenderInMainPass(true);
		VisualTile->SetOwnerNoSee(false);
		VisualTile->SetOnlyOwnerSee(false);
		VisualTiles.Add(VisualTile);

		const FName CollisionName(*FString::Printf(TEXT("PhysicalTile_%02d"), TileIndex));
		UBoxComponent* const PhysicalTile = CreateDefaultSubobject<UBoxComponent>(CollisionName);
		PhysicalTile->SetupAttachment(SceneRoot);
		PhysicalTile->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		PhysicalTile->SetCollisionResponseToAllChannels(ECR_Block);
		PhysicalTile->SetGenerateOverlapEvents(false);
		PhysicalTile->SetCanEverAffectNavigation(true);
		PhysicalTile->SetRelativeLocation(FVector(0.0f, 0.0f, GroundSurfaceZ - FMath::Max(1.0f, CollisionThickness) * 0.5f));
		PhysicalTiles.Add(PhysicalTile);
	}
}

void AJTSMoonLoopGroundActor::BeginPlay()
{
	Super::BeginPlay();

	if (!IsMoonWorld())
	{
		SetMoonWorldEnabled(false);
		SetActorTickEnabled(false);
		return;
	}

	SetMoonWorldEnabled(true);
	RebuildTiles();
	ApplyVisualMaterial();

	if (bEnableDebugLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("JTSMoonLoopGroundActor initialized with %d reusable flat tiles of %.1f x %.1f cm."), TileRingCount, CachedMapSize.X, CachedMapSize.Y);
	}
}

void AJTSMoonLoopGroundActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ResolveMapSize();
	BuildVisualMeshes();
	InvalidateTileAssignments();
	bMoonWorldEnabled = true;
	SetMoonWorldEnabled(true);
	UpdateTileRing(GetActorLocation(), nullptr);
	ApplyVisualMaterial();
}

#if WITH_EDITOR
void AJTSMoonLoopGroundActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(AJTSMoonLoopGroundActor, VisualMaterial))
	{
		ApplyVisualMaterial();
	}
}
#endif

void AJTSMoonLoopGroundActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bMoonWorldEnabled)
	{
		if (!IsMoonWorld())
		{
			return;
		}

		bMoonWorldEnabled = true;
		SetMoonWorldEnabled(true);
	}

	UWorld* const World = GetWorld();
	const FVector2D PreviousMapSize = CachedMapSize;
	ResolveMapSize();
	if (!PreviousMapSize.Equals(CachedMapSize, 0.01f))
	{
		BuildVisualMeshes();
		InvalidateTileAssignments();
	}

	APlayerController* const PlayerController = World != nullptr ? World->GetFirstPlayerController() : nullptr;
	APawn* const LocalPawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
	if (LocalPawn != nullptr)
	{
		if (!bVisualMeshesBuilt)
		{
			BuildVisualMeshes();
		}
		UpdateTileRing(LocalPawn->GetActorLocation(), GetMovementBaseForPawn(LocalPawn));
	}
}

void AJTSMoonLoopGroundActor::RebuildTiles()
{
	UWorld* const World = GetWorld();
	const bool bIsEditorPreviewWorld = World != nullptr && !World->IsGameWorld();
	if (!IsMoonWorld() && !bIsEditorPreviewWorld)
	{
		SetMoonWorldEnabled(false);
		return;
	}

	bMoonWorldEnabled = true;
	SetMoonWorldEnabled(true);
	ResolveMapSize();
	BuildVisualMeshes();
	InvalidateTileAssignments();

	APlayerController* const PlayerController = World != nullptr ? World->GetFirstPlayerController() : nullptr;
	APawn* const LocalPawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
	UpdateTileRing(
		LocalPawn != nullptr ? LocalPawn->GetActorLocation() : GetActorLocation(),
		GetMovementBaseForPawn(LocalPawn));
	ApplyVisualMaterial();
}

bool AJTSMoonLoopGroundActor::IsMoonWorld() const
{
	const UWorld* const World = GetWorld();
	if (World == nullptr || World->GetAuthGameMode<AJTSMoonGameMode>() == nullptr)
	{
		return false;
	}

	for (TActorIterator<AJTSMoonWorldActor> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			return true;
		}
	}

	return false;
}

void AJTSMoonLoopGroundActor::SetMoonWorldEnabled(bool bEnabled)
{
	for (UProceduralMeshComponent* const VisualTile : VisualTiles)
	{
		if (IsValid(VisualTile))
		{
			if (bEnabled)
			{
				ConfigureVisualTileRenderState(VisualTile);
			}
			else
			{
				VisualTile->SetVisibility(false, true);
			}
		}
	}
	bMoonWorldEnabled = bEnabled;

	for (UBoxComponent* const PhysicalTile : PhysicalTiles)
	{
		if (IsValid(PhysicalTile))
		{
			PhysicalTile->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
			PhysicalTile->SetVisibility(bEnabled, true);
		}
	}
}

FIntPoint AJTSMoonLoopGroundActor::GetCurrentTile() const
{
	return CurrentTile;
}

int32 AJTSMoonLoopGroundActor::GetTileCount() const
{
	return TileRingCount;
}

void AJTSMoonLoopGroundActor::ResolveMapSize()
{
	if (const UWorld* const World = GetWorld())
	{
		if (const UJTSMoonWrapSubsystem* const WrapSubsystem = World->GetSubsystem<UJTSMoonWrapSubsystem>())
		{
			CachedMapSize = WrapSubsystem->GetMapSize2D();
		}
	}

	CachedMapSize.X = FMath::IsFinite(CachedMapSize.X) ? FMath::Max(1.0f, CachedMapSize.X) : 24000.0f;
	CachedMapSize.Y = FMath::IsFinite(CachedMapSize.Y) ? FMath::Max(1.0f, CachedMapSize.Y) : 24000.0f;
}

void AJTSMoonLoopGroundActor::BuildVisualMeshes()
{
	const int32 XSegments = FMath::Max(1, FMath::CeilToInt(CachedMapSize.X / FMath::Max(1.0f, VisualGridCellSize)));
	const int32 YSegments = FMath::Max(1, FMath::CeilToInt(CachedMapSize.Y / FMath::Max(1.0f, VisualGridCellSize)));
	const int32 VertexCount = (XSegments + 1) * (YSegments + 1);
	const int32 TriangleIndexCount = XSegments * YSegments * 6;

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FLinearColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;
	Vertices.Reserve(VertexCount);
	Normals.Reserve(VertexCount);
	UV0.Reserve(VertexCount);
	VertexColors.Reserve(VertexCount);
	Tangents.Reserve(VertexCount);
	Triangles.Reserve(TriangleIndexCount);

	const float HalfX = CachedMapSize.X * 0.5f;
	const float HalfY = CachedMapSize.Y * 0.5f;
	for (int32 Y = 0; Y <= YSegments; ++Y)
	{
		const float V = static_cast<float>(Y) / static_cast<float>(YSegments);
		for (int32 X = 0; X <= XSegments; ++X)
		{
			const float U = static_cast<float>(X) / static_cast<float>(XSegments);
			Vertices.Add(FVector(FMath::Lerp(-HalfX, HalfX, U), FMath::Lerp(-HalfY, HalfY, V), GroundSurfaceZ));
			Normals.Add(FVector(0.0f, 0.0f, 1.0f));
			UV0.Add(FVector2D(U, V));
			VertexColors.Add(FLinearColor::White);
			Tangents.Add(FProcMeshTangent(FVector(1.0f, 0.0f, 0.0f), false));
		}
	}

	for (int32 Y = 0; Y < YSegments; ++Y)
	{
		for (int32 X = 0; X < XSegments; ++X)
		{
			const int32 BottomLeft = Y * (XSegments + 1) + X;
			const int32 BottomRight = BottomLeft + 1;
			const int32 TopLeft = BottomLeft + XSegments + 1;
			const int32 TopRight = TopLeft + 1;
			Triangles.Add(BottomLeft);
			Triangles.Add(TopRight);
			Triangles.Add(BottomRight);
			Triangles.Add(BottomLeft);
			Triangles.Add(TopLeft);
			Triangles.Add(TopRight);
		}
	}

	const float CollisionHalfHeight = FMath::Max(1.0f, CollisionThickness) * 0.5f;
	for (int32 TileIndex = 0; TileIndex < TileRingCount; ++TileIndex)
	{
		if (UProceduralMeshComponent* const VisualTile = VisualTiles.IsValidIndex(TileIndex) ? VisualTiles[TileIndex] : nullptr)
		{
			VisualTile->ClearAllMeshSections();
			VisualTile->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UV0, VertexColors, Tangents, false);
			if (VisualMaterial != nullptr)
			{
				VisualTile->SetMaterial(0, VisualMaterial);
			}
			else
			{
				VisualTile->SetMaterial(0, UMaterial::GetDefaultMaterial(MD_Surface));
			}
			VisualTile->SetMeshSectionVisible(0, true);
			VisualTile->SetBoundsScale(GetVisualBoundsScale());
			ConfigureVisualTileRenderState(VisualTile);

			const FProcMeshSection* const Section = VisualTile->GetProcMeshSection(0);
			const bool bHasValidSection = VisualTile->GetNumSections() > 0
				&& Section != nullptr
				&& Section->ProcVertexBuffer.Num() > 0
				&& Section->ProcIndexBuffer.Num() > 0;
			ensureMsgf(bHasValidSection, TEXT("MoonLoopGround VisualTile_%02d has no valid section 0 after mesh creation."), TileIndex);
		}

		if (TileIndex == 0)
		{
			LogVisualTileDiagnostics();
		}

		if (UBoxComponent* const PhysicalTile = PhysicalTiles.IsValidIndex(TileIndex) ? PhysicalTiles[TileIndex] : nullptr)
		{
			PhysicalTile->SetBoxExtent(FVector(HalfX, HalfY, CollisionHalfHeight), false);
		}
	}

	ApplyVisualMaterial();
	bVisualMeshesBuilt = true;
}

void AJTSMoonLoopGroundActor::ApplyVisualMaterial()
{
	UMaterialInterface* EffectiveMaterial = nullptr;
	if (VisualMaterial != nullptr)
	{
		EffectiveMaterial = VisualMaterial.Get();
	}
	else
	{
		EffectiveMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	}

	for (UProceduralMeshComponent* const VisualTile : VisualTiles)
	{
		if (IsValid(VisualTile))
		{
			VisualTile->SetMaterial(0, EffectiveMaterial);
			VisualTile->MarkRenderStateDirty();
		}
	}
}

void AJTSMoonLoopGroundActor::ConfigureVisualTileRenderState(UProceduralMeshComponent* VisualTile) const
{
	if (!IsValid(VisualTile))
	{
		return;
	}

	VisualTile->SetVisibility(true, true);
	VisualTile->SetHiddenInGame(false);
	VisualTile->SetRenderInMainPass(true);
	VisualTile->SetOwnerNoSee(false);
	VisualTile->SetOnlyOwnerSee(false);
	if (VisualTile->GetNumSections() > 0)
	{
		VisualTile->SetMeshSectionVisible(0, true);
	}
	VisualTile->MarkRenderStateDirty();
}

void AJTSMoonLoopGroundActor::LogVisualTileDiagnostics() const
{
	#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	return;
	#else
	UProceduralMeshComponent* const VisualTile = VisualTiles.IsValidIndex(0) ? VisualTiles[0] : nullptr;
	if (!IsValid(VisualTile))
	{
		UE_LOG(LogTemp, Warning, TEXT("MoonLoopGround VisualTile_00: component is unavailable."));
		return;
	}

	const FProcMeshSection* const Section = VisualTile->GetProcMeshSection(0);
	const int32 VertexCount = Section != nullptr ? Section->ProcVertexBuffer.Num() : 0;
	const int32 IndexCount = Section != nullptr ? Section->ProcIndexBuffer.Num() : 0;
	const UMaterialInterface* const Material0 = VisualTile->GetMaterial(0);
	const FString Material0Name = IsValid(Material0) ? Material0->GetName() : TEXT("null");
	int32 Triangle0A = INDEX_NONE;
	int32 Triangle0B = INDEX_NONE;
	int32 Triangle0C = INDEX_NONE;
	int32 Triangle1A = INDEX_NONE;
	int32 Triangle1B = INDEX_NONE;
	int32 Triangle1C = INDEX_NONE;
	if (Section != nullptr)
	{
		const TArray<uint32>& SectionIndices = Section->ProcIndexBuffer;
		Triangle0A = SectionIndices.IsValidIndex(0) ? static_cast<int32>(SectionIndices[0]) : INDEX_NONE;
		Triangle0B = SectionIndices.IsValidIndex(1) ? static_cast<int32>(SectionIndices[1]) : INDEX_NONE;
		Triangle0C = SectionIndices.IsValidIndex(2) ? static_cast<int32>(SectionIndices[2]) : INDEX_NONE;
		Triangle1A = SectionIndices.IsValidIndex(3) ? static_cast<int32>(SectionIndices[3]) : INDEX_NONE;
		Triangle1B = SectionIndices.IsValidIndex(4) ? static_cast<int32>(SectionIndices[4]) : INDEX_NONE;
		Triangle1C = SectionIndices.IsValidIndex(5) ? static_cast<int32>(SectionIndices[5]) : INDEX_NONE;
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("MoonLoopGround VisualTile_00: Triangle1=(%d,%d,%d) Triangle2=(%d,%d,%d) Material0=%s Sections=%d Vertices=%d Indices=%d"),
		Triangle0A,
		Triangle0B,
		Triangle0C,
		Triangle1A,
		Triangle1B,
		Triangle1C,
		*Material0Name,
		VisualTile->GetNumSections(),
		VertexCount,
		IndexCount);
	#endif
}

void AJTSMoonLoopGroundActor::InvalidateTileAssignments()
{
	AssignedTileCoordinates.Init(InvalidTileCoordinate, TileRingCount);
	CurrentTile = FIntPoint::ZeroValue;
	bHasCurrentTile = false;
}

void AJTSMoonLoopGroundActor::UpdateTileRing(const FVector& PlayerLocation, UPrimitiveComponent* MovementBase)
{
	const FIntPoint DesiredCenter = CalculateTileCoordinate(PlayerLocation);
	if (bHasCurrentTile && DesiredCenter == CurrentTile)
	{
		return;
	}

	if (!bHasCurrentTile)
	{
		if (InitializeTileRing(DesiredCenter, MovementBase))
		{
			CurrentTile = DesiredCenter;
			bHasCurrentTile = true;
			ValidateTileAssignments(CurrentTile);
		}
		return;
	}

	const FIntPoint PreviousCenter = CurrentTile;
	const FIntPoint NextCenter(
		CurrentTile.X + FMath::Clamp(DesiredCenter.X - CurrentTile.X, -1, 1),
		CurrentTile.Y + FMath::Clamp(DesiredCenter.Y - CurrentTile.Y, -1, 1));
	UE_LOG(
		LogTemp,
		Log,
		TEXT("MoonLoopGround center tile change requested: PlayerPhysicalXY=(%.1f,%.1f) OldCenterTileCoord=(%d,%d) NewCenterTileCoord=(%d,%d) MovementBase=%s."),
		PlayerLocation.X,
		PlayerLocation.Y,
		PreviousCenter.X,
		PreviousCenter.Y,
		NextCenter.X,
		NextCenter.Y,
		*GetNameSafe(MovementBase));
	if (!RecycleTiles(NextCenter, MovementBase))
	{
		return;
	}

	CurrentTile = NextCenter;
	bHasCurrentTile = true;
	ValidateTileAssignments(CurrentTile);
}

bool AJTSMoonLoopGroundActor::InitializeTileRing(const FIntPoint& DesiredCenter, UPrimitiveComponent* MovementBase)
{
	TArray<int32> PairIndices;
	TArray<FIntPoint> DestinationCoordinates;
	PairIndices.Reserve(TileRingCount);
	DestinationCoordinates.Reserve(TileRingCount);

	for (int32 RingY = -1; RingY <= 1; ++RingY)
	{
		for (int32 RingX = -1; RingX <= 1; ++RingX)
		{
			DestinationCoordinates.Add(DesiredCenter + FIntPoint(RingX, RingY));
		}
	}
	for (int32 PairIndex = 0; PairIndex < TileRingCount; ++PairIndex)
	{
		PairIndices.Add(PairIndex);
	}

	const int32 MovementBasePairIndex = FindTilePairForPhysicalComponent(MovementBase);
	if (MovementBasePairIndex != INDEX_NONE)
	{
		const UBoxComponent* const MovementBaseTile = PhysicalTiles[MovementBasePairIndex];
		const FIntPoint MovementBaseCoordinate = CalculateTileCoordinate(MovementBaseTile->GetComponentLocation());
		const int32 MovementBaseDestinationIndex = DestinationCoordinates.IndexOfByKey(MovementBaseCoordinate);
		if (MovementBaseDestinationIndex == INDEX_NONE)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("MoonLoopGround cannot initialize its ring without moving the player's current movement base. PhysicalTile=%s Pair=%d CurrentWorld=(%.1f,%.1f,%.1f) DesiredCenter=(%d,%d)."),
				*GetNameSafe(MovementBase),
				MovementBasePairIndex,
				MovementBaseTile->GetComponentLocation().X,
				MovementBaseTile->GetComponentLocation().Y,
				MovementBaseTile->GetComponentLocation().Z,
				DesiredCenter.X,
				DesiredCenter.Y);
			ensureMsgf(false, TEXT("Attempted to recycle player's current movement base."));
			return false;
		}

		PairIndices.RemoveSingle(MovementBasePairIndex);
		DestinationCoordinates.RemoveAt(MovementBaseDestinationIndex);
		PairIndices.Insert(MovementBasePairIndex, 0);
		DestinationCoordinates.Insert(MovementBaseCoordinate, 0);
	}

	return ReassignTilePairs(PairIndices, DestinationCoordinates, MovementBase, false);
}

bool AJTSMoonLoopGroundActor::RecycleTiles(const FIntPoint& DesiredCenter, UPrimitiveComponent* MovementBase)
{
	if (!ValidateTileAssignments(CurrentTile))
	{
		UE_LOG(LogTemp, Error, TEXT("MoonLoopGround cannot recycle because its current tile assignments are invalid."));
		return false;
	}

	const FIntPoint CenterDelta = DesiredCenter - CurrentTile;
	if (FMath::Abs(CenterDelta.X) > 1 || FMath::Abs(CenterDelta.Y) > 1)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("MoonLoopGround cannot recycle a non-adjacent center change from (%d,%d) to (%d,%d)."),
			CurrentTile.X,
			CurrentTile.Y,
			DesiredCenter.X,
			DesiredCenter.Y);
		ensureMsgf(false, TEXT("MoonLoopGround recycle requires adjacent center coordinates."));
		return false;
	}

	TArray<int32> PairIndicesToMove;
	TArray<FIntPoint> DestinationCoordinates;
	PairIndicesToMove.Reserve(TileRingCount);
	DestinationCoordinates.Reserve(TileRingCount);
	TSet<FIntPoint> NewRingCoordinates;
	NewRingCoordinates.Reserve(TileRingCount);

	for (int32 PairIndex = 0; PairIndex < TileRingCount; ++PairIndex)
	{
		const FIntPoint SourceCoordinate = AssignedTileCoordinates[PairIndex];
		FIntPoint NewRelativeCoordinate = SourceCoordinate - CurrentTile - CenterDelta;
		if (NewRelativeCoordinate.X < -1)
		{
			NewRelativeCoordinate.X = 1;
		}
		else if (NewRelativeCoordinate.X > 1)
		{
			NewRelativeCoordinate.X = -1;
		}

		if (NewRelativeCoordinate.Y < -1)
		{
			NewRelativeCoordinate.Y = 1;
		}
		else if (NewRelativeCoordinate.Y > 1)
		{
			NewRelativeCoordinate.Y = -1;
		}

		const FIntPoint DestinationCoordinate = DesiredCenter + NewRelativeCoordinate;
		if (NewRingCoordinates.Contains(DestinationCoordinate))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("Duplicate MoonLoopGround tile coordinate. Recycle maps multiple pairs to (%d,%d)."),
				DestinationCoordinate.X,
				DestinationCoordinate.Y);
			ensureMsgf(false, TEXT("Duplicate MoonLoopGround tile coordinate."));
			return false;
		}
		NewRingCoordinates.Add(DestinationCoordinate);

		if (DestinationCoordinate != SourceCoordinate)
		{
			PairIndicesToMove.Add(PairIndex);
			DestinationCoordinates.Add(DestinationCoordinate);
		}
	}

	return ReassignTilePairs(PairIndicesToMove, DestinationCoordinates, MovementBase, true);
}

bool AJTSMoonLoopGroundActor::ReassignTilePairs(
	const TArray<int32>& PairIndices,
	const TArray<FIntPoint>& DestinationCoordinates,
	UPrimitiveComponent* MovementBase,
	bool bLogTransitions)
{
	if (PairIndices.Num() != DestinationCoordinates.Num())
	{
		UE_LOG(LogTemp, Error, TEXT("MoonLoopGround tile-pair reassignment has mismatched source and destination counts."));
		ensureMsgf(false, TEXT("MoonLoopGround tile-pair reassignment mismatch."));
		return false;
	}

	for (int32 AssignmentIndex = 0; AssignmentIndex < PairIndices.Num(); ++AssignmentIndex)
	{
		if (!CanPositionTilePair(PairIndices[AssignmentIndex], DestinationCoordinates[AssignmentIndex], MovementBase))
		{
			return false;
		}
	}

	for (int32 AssignmentIndex = 0; AssignmentIndex < PairIndices.Num(); ++AssignmentIndex)
	{
		if (!PositionTilePair(PairIndices[AssignmentIndex], DestinationCoordinates[AssignmentIndex], bLogTransitions))
		{
			return false;
		}
	}

	return true;
}

bool AJTSMoonLoopGroundActor::CanPositionTilePair(
	int32 PairIndex,
	const FIntPoint& TileCoordinate,
	UPrimitiveComponent* MovementBase) const
{
	if (!AssignedTileCoordinates.IsValidIndex(PairIndex))
	{
		return false;
	}

	const UProceduralMeshComponent* const VisualTile = VisualTiles.IsValidIndex(PairIndex) ? VisualTiles[PairIndex] : nullptr;
	const UBoxComponent* const PhysicalTile = PhysicalTiles.IsValidIndex(PairIndex) ? PhysicalTiles[PairIndex] : nullptr;
	if (!IsValid(VisualTile) || !IsValid(PhysicalTile))
	{
		UE_LOG(LogTemp, Error, TEXT("MoonLoopGround tile pair %d is missing a visual or physical component before recycle."), PairIndex);
		ensureMsgf(false, TEXT("MoonLoopGround tile pair is incomplete before recycle."));
		return false;
	}

	if (PhysicalTile != MovementBase)
	{
		return true;
	}

	const FIntPoint OldCoordinate = AssignedTileCoordinates[PairIndex];
	const FVector NewPhysicalWorldLocation = GetPhysicalTileWorldLocation(TileCoordinate);
	const bool bRecyclingAssignedTile = OldCoordinate != InvalidTileCoordinate && OldCoordinate != TileCoordinate;
	const bool bPhysicalTileWouldMove = !PhysicalTile->GetComponentLocation().Equals(NewPhysicalWorldLocation, TileTransformTolerance);
	if (!bRecyclingAssignedTile && !bPhysicalTileWouldMove)
	{
		return true;
	}

	const FVector OldPhysicalWorldLocation = PhysicalTile->GetComponentLocation();
	UE_LOG(
		LogTemp,
		Error,
		TEXT("Attempted to recycle player's current movement base. PhysicalTile=%s Pair=%d OldTileCoord=(%d,%d) NewTileCoord=(%d,%d) OldWorld=(%.1f,%.1f,%.1f) NewWorld=(%.1f,%.1f,%.1f)."),
		*GetNameSafe(PhysicalTile),
		PairIndex,
		OldCoordinate.X,
		OldCoordinate.Y,
		TileCoordinate.X,
		TileCoordinate.Y,
		OldPhysicalWorldLocation.X,
		OldPhysicalWorldLocation.Y,
		OldPhysicalWorldLocation.Z,
		NewPhysicalWorldLocation.X,
		NewPhysicalWorldLocation.Y,
		NewPhysicalWorldLocation.Z);
	ensureMsgf(false, TEXT("Attempted to recycle player's current movement base."));
	return false;
}

bool AJTSMoonLoopGroundActor::PositionTilePair(int32 PairIndex, const FIntPoint& TileCoordinate, bool bLogTransition)
{
	if (!AssignedTileCoordinates.IsValidIndex(PairIndex))
	{
		return false;
	}

	UProceduralMeshComponent* const VisualTile = VisualTiles.IsValidIndex(PairIndex) ? VisualTiles[PairIndex] : nullptr;
	UBoxComponent* const PhysicalTile = PhysicalTiles.IsValidIndex(PairIndex) ? PhysicalTiles[PairIndex] : nullptr;
	if (!IsValid(VisualTile) || !IsValid(PhysicalTile))
	{
		UE_LOG(LogTemp, Error, TEXT("MoonLoopGround tile pair %d is missing a visual or physical component."), PairIndex);
		ensureMsgf(false, TEXT("MoonLoopGround tile pair is incomplete."));
		return false;
	}

	const FIntPoint OldCoordinate = AssignedTileCoordinates[PairIndex];
	const FVector OldVisualWorldLocation = VisualTile->GetComponentLocation();
	const FVector OldPhysicalWorldLocation = PhysicalTile->GetComponentLocation();
	const FVector NewVisualWorldLocation = GetTileWorldLocation(TileCoordinate);
	const FVector NewPhysicalWorldLocation = GetPhysicalTileWorldLocation(TileCoordinate);
	if (!OldVisualWorldLocation.Equals(NewVisualWorldLocation, TileTransformTolerance))
	{
		VisualTile->SetWorldLocation(NewVisualWorldLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}
	if (!OldPhysicalWorldLocation.Equals(NewPhysicalWorldLocation, TileTransformTolerance))
	{
		PhysicalTile->SetWorldLocation(NewPhysicalWorldLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}

	AssignedTileCoordinates[PairIndex] = TileCoordinate;
	ConfigureVisualTileRenderState(VisualTile);

	if (bLogTransition && OldCoordinate != TileCoordinate)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("MoonLoopGround recycled tile pair: VisualTile=%s PhysicalTile=%s OldTileCoord=(%d,%d) NewTileCoord=(%d,%d) VisualWorld=(%.1f,%.1f,%.1f)->(%.1f,%.1f,%.1f) PhysicalWorld=(%.1f,%.1f,%.1f)->(%.1f,%.1f,%.1f)."),
			*GetNameSafe(VisualTile),
			*GetNameSafe(PhysicalTile),
			OldCoordinate.X,
			OldCoordinate.Y,
			TileCoordinate.X,
			TileCoordinate.Y,
			OldVisualWorldLocation.X,
			OldVisualWorldLocation.Y,
			OldVisualWorldLocation.Z,
			NewVisualWorldLocation.X,
			NewVisualWorldLocation.Y,
			NewVisualWorldLocation.Z,
			OldPhysicalWorldLocation.X,
			OldPhysicalWorldLocation.Y,
			OldPhysicalWorldLocation.Z,
			NewPhysicalWorldLocation.X,
			NewPhysicalWorldLocation.Y,
			NewPhysicalWorldLocation.Z);
	}

	return true;
}

bool AJTSMoonLoopGroundActor::ValidateTileAssignments(const FIntPoint& ExpectedCenter) const
{
	bool bAssignmentsAreValid = AssignedTileCoordinates.Num() == TileRingCount;
	if (!bAssignmentsAreValid)
	{
		UE_LOG(LogTemp, Error, TEXT("MoonLoopGround has %d assigned tile coordinates; expected %d."), AssignedTileCoordinates.Num(), TileRingCount);
		ensureMsgf(false, TEXT("MoonLoopGround tile-coordinate count is invalid."));
		return false;
	}

	for (int32 PairIndex = 0; PairIndex < TileRingCount; ++PairIndex)
	{
		const FIntPoint TileCoordinate = AssignedTileCoordinates[PairIndex];
		if (TileCoordinate == InvalidTileCoordinate || !IsTileCoordinateInRing(TileCoordinate, ExpectedCenter))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("MoonLoopGround tile pair %d has an invalid coordinate (%d,%d) for center (%d,%d)."),
				PairIndex,
				TileCoordinate.X,
				TileCoordinate.Y,
				ExpectedCenter.X,
				ExpectedCenter.Y);
			ensureMsgf(false, TEXT("MoonLoopGround tile coordinate is outside the active ring."));
			bAssignmentsAreValid = false;
		}

		const UProceduralMeshComponent* const VisualTile = VisualTiles.IsValidIndex(PairIndex) ? VisualTiles[PairIndex] : nullptr;
		const UBoxComponent* const PhysicalTile = PhysicalTiles.IsValidIndex(PairIndex) ? PhysicalTiles[PairIndex] : nullptr;
		if (!IsValid(VisualTile) || !IsValid(PhysicalTile))
		{
			UE_LOG(LogTemp, Error, TEXT("MoonLoopGround tile pair %d is missing a visual or physical component."), PairIndex);
			ensureMsgf(false, TEXT("MoonLoopGround tile pair is incomplete."));
			bAssignmentsAreValid = false;
			continue;
		}

		const FVector ExpectedVisualWorldLocation = GetTileWorldLocation(TileCoordinate);
		const FVector ExpectedPhysicalWorldLocation = GetPhysicalTileWorldLocation(TileCoordinate);
		if (!VisualTile->GetComponentLocation().Equals(ExpectedVisualWorldLocation, TileTransformTolerance)
			|| !PhysicalTile->GetComponentLocation().Equals(ExpectedPhysicalWorldLocation, TileTransformTolerance))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("MoonLoopGround tile pair %d transform does not match assigned coordinate (%d,%d)."),
				PairIndex,
				TileCoordinate.X,
				TileCoordinate.Y);
			ensureMsgf(false, TEXT("MoonLoopGround tile transform does not match its assigned coordinate."));
			bAssignmentsAreValid = false;
		}

		for (int32 OtherPairIndex = PairIndex + 1; OtherPairIndex < TileRingCount; ++OtherPairIndex)
		{
			if (TileCoordinate == AssignedTileCoordinates[OtherPairIndex])
			{
				UE_LOG(
					LogTemp,
					Error,
					TEXT("Duplicate MoonLoopGround tile coordinate. Pair %d and pair %d both use (%d,%d)."),
					PairIndex,
					OtherPairIndex,
					TileCoordinate.X,
					TileCoordinate.Y);
				ensureMsgf(false, TEXT("Duplicate MoonLoopGround tile coordinate."));
				bAssignmentsAreValid = false;
			}

			const UProceduralMeshComponent* const OtherVisualTile = VisualTiles.IsValidIndex(OtherPairIndex) ? VisualTiles[OtherPairIndex] : nullptr;
			if (IsValid(OtherVisualTile)
				&& VisualTile->GetComponentLocation().Equals(OtherVisualTile->GetComponentLocation(), TileTransformTolerance))
			{
				UE_LOG(
					LogTemp,
					Error,
					TEXT("MoonLoopGround visual tiles %d and %d have duplicate world centers."),
					PairIndex,
					OtherPairIndex);
				ensureMsgf(false, TEXT("Duplicate MoonLoopGround visual tile world center."));
				bAssignmentsAreValid = false;
			}
		}
	}

	return bAssignmentsAreValid;
}

bool AJTSMoonLoopGroundActor::IsTileCoordinateInRing(const FIntPoint& TileCoordinate, const FIntPoint& CenterTile) const
{
	return TileCoordinate.X >= CenterTile.X - 1
		&& TileCoordinate.X <= CenterTile.X + 1
		&& TileCoordinate.Y >= CenterTile.Y - 1
		&& TileCoordinate.Y <= CenterTile.Y + 1;
}

int32 AJTSMoonLoopGroundActor::FindTilePairForPhysicalComponent(const UPrimitiveComponent* PhysicalComponent) const
{
	if (!IsValid(PhysicalComponent))
	{
		return INDEX_NONE;
	}

	for (int32 PairIndex = 0; PairIndex < PhysicalTiles.Num(); ++PairIndex)
	{
		if (PhysicalTiles[PairIndex] == PhysicalComponent)
		{
			return PairIndex;
		}
	}

	return INDEX_NONE;
}
UPrimitiveComponent* AJTSMoonLoopGroundActor::GetMovementBaseForPawn(APawn* LocalPawn) const
{
	ACharacter* const LocalCharacter = Cast<ACharacter>(LocalPawn);
	UCharacterMovementComponent* const MoveComp = LocalCharacter != nullptr ? LocalCharacter->GetCharacterMovement() : nullptr;
	return MoveComp != nullptr ? Cast<UPrimitiveComponent>(MoveComp->GetMovementBaseObject()) : nullptr;
}

float AJTSMoonLoopGroundActor::GetVisualBoundsScale() const
{
	const float BaseBoundsRadius = FMath::Sqrt(
		FMath::Square(CachedMapSize.X * 0.5f)
		+ FMath::Square(CachedMapSize.Y * 0.5f));
	if (const UWorld* const World = GetWorld())
	{
		for (TActorIterator<AJTSMoonWorldActor> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				return (*It)->GetRecommendedBoundsScale(BaseBoundsRadius);
			}
		}
	}
	return 1.25f;
}

FIntPoint AJTSMoonLoopGroundActor::CalculateTileCoordinate(const FVector& PlayerLocation) const
{
	const float HalfX = CachedMapSize.X * 0.5f;
	const float HalfY = CachedMapSize.Y * 0.5f;
	return FIntPoint(
		FMath::FloorToInt((PlayerLocation.X - GetActorLocation().X + HalfX) / CachedMapSize.X),
		FMath::FloorToInt((PlayerLocation.Y - GetActorLocation().Y + HalfY) / CachedMapSize.Y));
}

FVector AJTSMoonLoopGroundActor::GetTileWorldLocation(const FIntPoint& TileCoordinate) const
{
	const FVector SheetOrigin = GetActorLocation();
	return FVector(
		SheetOrigin.X + static_cast<float>(TileCoordinate.X) * CachedMapSize.X,
		SheetOrigin.Y + static_cast<float>(TileCoordinate.Y) * CachedMapSize.Y,
		SheetOrigin.Z + GroundSurfaceZ);
}

FVector AJTSMoonLoopGroundActor::GetPhysicalTileWorldLocation(const FIntPoint& TileCoordinate) const
{
	return GetTileWorldLocation(TileCoordinate)
		+ FVector(0.0f, 0.0f, -FMath::Max(1.0f, CollisionThickness) * 0.5f);
}
