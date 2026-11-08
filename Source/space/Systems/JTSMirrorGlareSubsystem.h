#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "JTSMirrorGlareSubsystem.generated.h"

class UJTSMirrorDefenseComponent;
class UJTSMirrorGlareWidget;
class USceneCaptureComponentCube;
class UTextureRenderTargetCube;

/** One local screen compositor per viewer; defence itself remains server owned. */
UCLASS()
class SPACE_API UJTSMirrorGlareSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	void RegisterSource(UJTSMirrorDefenseComponent* Source);
	void UnregisterSource(UJTSMirrorDefenseComponent* Source);
	const TArray<TWeakObjectPtr<UJTSMirrorDefenseComponent>>& GetSources() const { return Sources; }
	virtual void Tick(float Dt) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UJTSMirrorGlareSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickable() const override;
	virtual void Deinitialize() override;
private:
	TArray<TWeakObjectPtr<UJTSMirrorDefenseComponent>> Sources;
	UPROPERTY(Transient) TArray<TObjectPtr<UJTSMirrorGlareWidget>> Widgets;
	UPROPERTY(Transient) TObjectPtr<USceneCaptureComponentCube> ReflectionCapture;
	UPROPERTY(Transient) TObjectPtr<UTextureRenderTargetCube> ReflectionTexture;
	TWeakObjectPtr<UJTSMirrorDefenseComponent> ReflectedSource;
	float ReflectionCountdown = 0;
	void UpdateNearbyReflection(float Dt);
};
