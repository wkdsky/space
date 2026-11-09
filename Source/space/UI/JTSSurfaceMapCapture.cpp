#include "space/UI/JTSSurfaceMapCapture.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Math/RotationMatrix.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "space/UI/JTSNavigationMapModel.h"
#include "space/World/JTSCelestialNavigationTypes.h"
#include "space/World/JTSPlanetAnchor.h"

void UJTSSurfaceMapCapture::Update(UWorld* World, const FJTSNavigationMapFrame& Frame,
	const FJTSNavigationMapSettings& Settings, const AActor* Character, const AActor* Ship)
{
	if (!World || World->GetNetMode() == NM_DedicatedServer || !Frame.bGroundMap || !Frame.Terrain) return;
	const int32 Resolution = FMath::Clamp(Settings.GroundMapResolution, 128, 1024);
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
	if (Settings.GroundReliefMaterial && (!Relief || Relief->Parent != Settings.GroundReliefMaterial))
		Relief = UMaterialInstanceDynamic::Create(Settings.GroundReliefMaterial, this);
	if (!Settings.GroundReliefMaterial) Relief = nullptr;
	if (Relief && !Depth)
	{
		Depth = NewObject<UTextureRenderTarget2D>(this);
		Depth->ClearColor = FLinearColor::Black;
		Depth->RenderTargetFormat = RTF_R32f;
		Depth->bAutoGenerateMips = false;
		Depth->InitAutoFormat(Resolution, Resolution);
		Depth->UpdateResourceImmediate(true);
	}
	if (Depth && Depth->SizeX != Resolution) Depth->ResizeTarget(Resolution, Resolution);
	if (Relief && !DepthCapture)
	{
		DepthCapture = NewObject<USceneCaptureComponent2D>(this);
		DepthCapture->ProjectionType = ECameraProjectionMode::Orthographic;
		DepthCapture->CaptureSource = SCS_SceneDepth;
		DepthCapture->TextureTarget = Depth;
		DepthCapture->bCaptureEveryFrame = false;
		DepthCapture->bCaptureOnMovement = false;
		DepthCapture->bAutoCalculateOrthoPlanes = false;
		DepthCapture->bUpdateOrthoPlanes = false;
		DepthCapture->PostProcessBlendWeight = 0;
		DepthCapture->ShowFlags.SetTranslucency(false);
		DepthCapture->ShowFlags.SetFog(false);
		DepthCapture->ShowFlags.SetAtmosphere(false);
		DepthCapture->RegisterComponentWithWorld(World);
		DepthCapture->SetComponentTickEnabled(false);
	}
	Capture->HiddenActors.Reset();
	if (Character) Capture->HiddenActors.Add(const_cast<AActor*>(Character));
	if (Ship) Capture->HiddenActors.Add(const_cast<AActor*>(Ship));
	Capture->OrthoWidth = View.HalfWidth * 2;
	const float Height = FMath::Max(Settings.GroundCaptureHeight, View.HalfWidth * 1.5f);
	Capture->MaxViewDistanceOverride = Height + View.HalfWidth * 4;
	// One camera basis drives terrain and markers. No texture flip: screen right
	// matches the same radial-up cross view-forward direction used by movement.
	Capture->SetWorldLocationAndRotation(View.Origin + View.Up * Height,
		FRotationMatrix::MakeFromXZ(-View.Up, View.Forward).ToQuat());
	if (Relief && DepthCapture)
	{
		DepthCapture->HiddenActors = Capture->HiddenActors;
		DepthCapture->OrthoWidth = Capture->OrthoWidth;
		DepthCapture->MaxViewDistanceOverride = Capture->MaxViewDistanceOverride;
		DepthCapture->SetWorldTransform(Capture->GetComponentTransform());
		if (ReferencePlanet != View.Planet || ReferenceRadius <= 0)
		{
			ReferencePlanet = View.Planet;
			ReferenceRadius = FMath::RoundToFloat(View.Radius / 500.f) * 500.f;
		}
		// Work relative to the real planet centre to avoid large-world precision
		// loss and remove spherical curvature from elevation/contour shading.
		const FVector PlanetCenter = View.Planet.IsValid() ? View.Planet->GetPlanetCenter() : View.Origin - View.Up * View.Radius;
		Relief->SetTextureParameterValue(TEXT("Albedo"), Texture);
		Relief->SetTextureParameterValue(TEXT("Depth"), Depth);
		Relief->SetVectorParameterValue(TEXT("CaptureOrigin"), FLinearColor(View.Origin + View.Up * Height - PlanetCenter));
		Relief->SetVectorParameterValue(TEXT("RadialUp"), FLinearColor(View.Up));
		Relief->SetVectorParameterValue(TEXT("MapRight"), FLinearColor(View.Right));
		Relief->SetVectorParameterValue(TEXT("MapForward"), FLinearColor(View.Forward));
		Relief->SetScalarParameterValue(TEXT("MapSpan"), View.HalfWidth * 2);
		Relief->SetScalarParameterValue(TEXT("Texel"), 1.f / Resolution);
		Relief->SetScalarParameterValue(TEXT("ReferenceRadius"), ReferenceRadius);
		DepthCapture->CaptureSceneDeferred();
	}
	// Called from the existing local HUD update; no extra actor/component tick or polling timer.
	Capture->CaptureSceneDeferred();
}

void UJTSSurfaceMapCapture::Stop()
{
	if (Capture) Capture->DestroyComponent();
	if (DepthCapture) DepthCapture->DestroyComponent();
	Capture = nullptr;
	DepthCapture = nullptr;
}

UObject* UJTSSurfaceMapCapture::GetPresentationResource() const
{
	return Relief ? static_cast<UObject*>(Relief.Get()) : static_cast<UObject*>(Texture.Get());
}

void UJTSSurfaceMapCapture::BeginDestroy()
{
	Stop();
	Super::BeginDestroy();
}
