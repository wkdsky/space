#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "space/Items/JTSDiscoveryTypes.h"
#include "JTSCorpseMeshComponent.generated.h"

/** Frozen original creature triangles/materials, shared by world drops, the hand and the scanner.
 * No replacement model or authored corpse asset is created. Does no per-frame mesh work. */
UCLASS(ClassGroup=(Presentation))
class SPACE_API UJTSCorpseMeshComponent : public UProceduralMeshComponent
{
	GENERATED_BODY()
public:
	UJTSCorpseMeshComponent(const FObjectInitializer& ObjectInitializer);
	bool SetSample(const FJTSItemInstance& Item, const UJTSAnalysisCatalog* Catalog = nullptr);
	void PlaceAt(const FVector& Origin, bool bOnSurface, float MaximumSpan = 0);
	static TArray<FBox> GetPartBounds(const FJTSAnalysisEntry& Entry, UWorld* World);
	const FBox& GetSourceBounds() const { return SourceBounds; }
	bool RequiresTwoHandCarry(float MaxSpan, float MaxGripWidth) const;
private:
	UPROPERTY(Transient) TObjectPtr<USkeletalMesh> SourceMesh;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> SourcePose;
	FName CurrentSpecies;
	int32 CurrentPart = INDEX_NONE;
	FBox SourceBounds = FBox(ForceInit);
};
