#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "space/UI/FrontEnd/JTSAvatarTypes.h"
#include "JTSAvatarAppearanceComponent.generated.h"

/** Applies catalog identifiers to an existing mesh, without changing movement or collision. */
UCLASS(ClassGroup=(Presentation), meta=(BlueprintSpawnableComponent))
class SPACE_API UJTSAvatarAppearanceComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance") TObjectPtr<UJTSAvatarCatalog> Catalog;
 UFUNCTION(BlueprintCallable) void Apply(const FJTSAvatarAppearance& Appearance);
private:
 TWeakObjectPtr<class USkeletalMeshComponent> AppliedMesh;
 FJTSAvatarAppearance Last;
 bool bApplied = false;
};
