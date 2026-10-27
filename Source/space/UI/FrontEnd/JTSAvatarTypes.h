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
 UPROPERTY(EditAnywhere, BlueprintReadWrite) uint8 Helmet = 0;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) uint8 Upper = 1;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) uint8 Lower = 0;
 bool IsValid() const { return Suit < 5 && Eyes < 5 && Mouth < 5 && Helmet < 5 && Upper < 5 && Lower < 5; }
 bool operator==(const FJTSAvatarAppearance& B) const { return Suit==B.Suit && Eyes==B.Eyes && Mouth==B.Mouth && Helmet==B.Helmet && Upper==B.Upper && Lower==B.Lower; }
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
