#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "space/Interaction/IInteractable.h"
#include "JTSReadableDocument.generated.h"
class UStaticMeshComponent;
class UTexture2D;
UCLASS(BlueprintType)
class SPACE_API UJTSReadableDocumentData : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FName DocumentId;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FText Title;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(MultiLine=true)) FText Body;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) TObjectPtr<UTexture2D> Image;
};
UCLASS(Blueprintable)
class SPACE_API AJTSReadableDocument : public AActor,public IInteractable
{
    GENERATED_BODY()
public:
    AJTSReadableDocument();
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(EditAnywhere) TObjectPtr<UJTSReadableDocumentData> Document;
    UPROPERTY(EditAnywhere) FName ReadFlag;
    UPROPERTY(EditAnywhere, Category="Presentation") FText DisplayName;
    virtual bool CanInteract_Implementation(APawn* Pawn) const override;
    virtual FText GetInteractionPrompt_Implementation(APawn*) const override;
    virtual FText GetInteractionLabel_Implementation() const override { return DisplayName; }
    virtual void Interact_Implementation(APawn* Pawn) override;
};
