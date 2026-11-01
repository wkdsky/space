#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "JTSSurfaceMapCapture.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
struct FJTSNavigationMapFrame;

/** Local HUD resource. Orthographic view of real scene meshes/materials, aligned to spherical north. */
UCLASS(Transient)
class SPACE_API UJTSSurfaceMapCapture : public UObject
{
	GENERATED_BODY()
public:
	void Update(UWorld* World, const FJTSNavigationMapFrame& Frame, int32 Resolution, float Height,
		const AActor* Character, const AActor* Ship);
	void Stop();
	UTextureRenderTarget2D* GetTexture() const { return Texture; }
	virtual void BeginDestroy() override;
private:
	UPROPERTY(Transient) TObjectPtr<USceneCaptureComponent2D> Capture;
	UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> Texture;
};
