#include "space/UI/JTSSurfaceMapCapture.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Math/RotationMatrix.h"
#include "space/UI/JTSNavigationMapModel.h"

void UJTSSurfaceMapCapture::Update(UWorld* World, const FJTSNavigationMapFrame& Frame,
	int32 Resolution, float Height, const AActor* Character, const AActor* Ship)
{
	if (!World || World->GetNetMode() == NM_DedicatedServer || !Frame.bGroundMap || !Frame.Terrain) return;
	Resolution = FMath::Clamp(Resolution, 128, 1024);
	if (!Texture)
	{
		Texture = NewObject<UTextureRenderTarget2D>(this);
		Texture->ClearColor = FLinearColor(0.015f, 0.025f, 0.04f, 1);
		Texture->RenderTargetFormat = RTF_RGBA8;
		Texture->bAutoGenerateMips = false;
		Texture->InitAutoFormat(Resolution, Resolution);
		Texture->UpdateResourceImmediate(true);
	}
	else if (Texture->SizeX != Resolution) Texture->ResizeTarget(Resolution, Resolution);
	if (!Capture)
	{
		Capture = NewObject<USceneCaptureComponent2D>(this);
		Capture->ProjectionType = ECameraProjectionMode::Orthographic;
		Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
		// Capture material base color in the project's deferred renderer, including Shipping builds.
		// Lighting=false + FinalColor would depend on Unreal's development-only unlit view mode.
		Capture->CaptureSource = SCS_BaseColor;
		Capture->UnlitViewmode = ESceneCaptureUnlitViewmode::Disabled;
		Capture->bCaptureEveryFrame = false;
		Capture->bCaptureOnMovement = false;
		Capture->bAlwaysPersistRenderingState = false;
		Capture->bAutoCalculateOrthoPlanes = false;
		Capture->bUpdateOrthoPlanes = false;
		Capture->PostProcessBlendWeight = 0;
		// A readable albedo view in daylight and darkness, without atmosphere obscuring the ground.
		Capture->ShowFlags.SetLighting(false);
		Capture->ShowFlags.SetPostProcessing(false);
		Capture->ShowFlags.SetFog(false);
		Capture->ShowFlags.SetAtmosphere(false);
		Capture->ShowFlags.SetVolumetricFog(false);
		Capture->ShowFlags.SetDynamicShadows(false);
		Capture->ShowFlags.SetBloom(false);
		Capture->ShowFlags.SetMotionBlur(false);
		Capture->ShowFlags.SetTemporalAA(false);
		Capture->ShowFlags.SetAntiAliasing(true);
		Capture->ShowFlags.SetTranslucency(false);
		Capture->TextureTarget = Texture;
		Capture->RegisterComponentWithWorld(World);
		Capture->SetComponentTickEnabled(false);
	}
	const auto& View = *Frame.Terrain;
	Capture->HiddenActors.Reset();
	if (Character) Capture->HiddenActors.Add(const_cast<AActor*>(Character));
	if (Ship) Capture->HiddenActors.Add(const_cast<AActor*>(Ship));
	Capture->OrthoWidth = View.HalfWidth * 2;
	Height = FMath::Max(Height, View.HalfWidth * 1.5f);
	Capture->MaxViewDistanceOverride = Height + View.HalfWidth * 4;
	// Camera X looks down, Y points east and Z points south. Slate flips V for geographic north-up.
	Capture->SetWorldLocationAndRotation(View.Origin + View.Up * Height,
		FRotationMatrix::MakeFromXY(-View.Up, View.East).ToQuat());
	// Called from the existing local HUD update; no extra actor/component tick or polling timer.
	Capture->CaptureSceneDeferred();
}

void UJTSSurfaceMapCapture::Stop()
{
	if (Capture) Capture->DestroyComponent();
	Capture = nullptr;
}

void UJTSSurfaceMapCapture::BeginDestroy()
{
	Stop();
	Super::BeginDestroy();
}
