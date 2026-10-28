#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "JTSAvatarTypes.generated.h"

/** Small, server-validated identifiers; art remains editor-owned. */
USTRUCT(BlueprintType)
struct SPACE_API FJTSAvatarAppearance
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite) uint8 Suit = 0;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) uint8 Eyes = 0;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) uint8 Mouth = 1;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FColor HelmetColor = FColor(255, 128, 64);
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FColor UpperColor = FColor(64, 128, 255);
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FColor LowerColor = FColor(32, 32, 32);
 bool IsValid() const { return Suit < 5 && Eyes < 5 && Mouth < 5; }
 bool operator==(const FJTSAvatarAppearance& B) const { return Suit==B.Suit && Eyes==B.Eyes && Mouth==B.Mouth && HelmetColor==B.HelmetColor && UpperColor==B.UpperColor && LowerColor==B.LowerColor; }
};

UCLASS(BlueprintType)
class SPACE_API UJTSAvatarCatalog : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<TObjectPtr<USkeletalMesh>> Suits;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FLinearColor> Colors;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<class UAnimSequence> PreviewIdle;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<TObjectPtr<class UTexture2D>> SuitThumbnails;
};
