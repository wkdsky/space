#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "space/UI/FrontEnd/JTSAvatarTypes.h"
#include "JTSEntryPresentationStage.generated.h"

USTRUCT(BlueprintType)
struct FJTSMenuCameraView
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere) FTransform Transform;
 UPROPERTY(EditAnywhere) float FOV = 50.f;
};

/** Generic presentation anchor. All scenery and camera composition are editor-owned. */
UCLASS()
class SPACE_API AJTSEntryPresentationStage : public AActor
{
 GENERATED_BODY()
public:
 AJTSEntryPresentationStage();
 virtual void BeginPlay() override;
 UFUNCTION(BlueprintCallable) void ShowView(FName View);
 UFUNCTION(BlueprintCallable) void PreviewAppearance(const FJTSAvatarAppearance& Appearance);
 UPROPERTY(VisibleAnywhere) TObjectPtr<class UCameraComponent> PresentationCamera;
 UPROPERTY(VisibleAnywhere) TObjectPtr<class USkeletalMeshComponent> PreviewMesh;
 UPROPERTY(VisibleAnywhere) TObjectPtr<class UJTSAvatarAppearanceComponent> AppearanceComponent;
 UPROPERTY(EditAnywhere) TMap<FName,FJTSMenuCameraView> Views;
};
