#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "JTSMirrorGlareWidget.generated.h"

class UJTSMirrorDefenseComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS()
class SPACE_API UJTSMirrorGlareWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void InitializeMaterial(UMaterialInterface* Material);
	UFUNCTION(BlueprintPure, Category="Mirror") int32 GetIlluminatingMirrorCount() const { return IlluminatingCount; }
	UFUNCTION(BlueprintPure, Category="Mirror") float GetFullWhiteStrength() const { return FullOcclusion; }
	static float FullOcclusionForCount(int32 Count) { return Count >= 3 ? .35f : 0.f; }
protected:
	virtual void NativeTick(const FGeometry& Geometry, float Dt) override;
private:
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Glare;
	TWeakObjectPtr<UJTSMirrorDefenseComponent> Assigned[3];
	FVector2D Centers[3];
	float Strength[3] = {0,0,0};
	float ScanTime = 0;
	bool Illuminated[3] = {false,false,false};
	float FullOcclusion = 0;
	int32 IlluminatingCount = 0;

};
