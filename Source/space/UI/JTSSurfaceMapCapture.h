#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "JTSSurfaceMapCapture.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UMaterialInstanceDynamic;
struct FJTSNavigationMapFrame;
struct FJTSNavigationMapSettings;

/** Local orthographic albedo/depth captures; relief is presentation only. */
UCLASS(Transient)
class SPACE_API UJTSSurfaceMapCapture : public UObject
{
	GENERATED_BODY()
public:
	void Update(UWorld* World, const FJTSNavigationMapFrame& Frame, const FJTSNavigationMapSettings& Settings,
		const AActor* Character, const AActor* Ship);
	void Stop();
	UTextureRenderTarget2D* GetTexture() const { return Texture; }
	UObject* GetPresentationResource() const;
	virtual void BeginDestroy() override;
private:
	UPROPERTY(Transient) TObjectPtr<USceneCaptureComponent2D> Capture;
	UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> Texture;
	UPROPERTY(Transient) TObjectPtr<USceneCaptureComponent2D> DepthCapture;
	UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> Depth;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Relief;
	TWeakObjectPtr<const class AJTSPlanetAnchor> ReferencePlanet;
	float ReferenceRadius = 0;
};
