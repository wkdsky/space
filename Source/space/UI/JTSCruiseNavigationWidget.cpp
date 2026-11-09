#include "space/UI/JTSCruiseNavigationWidget.h"
#include "space/UI/SJTSNavigationMap.h"
#include "space/UI/JTSSurfaceMapCapture.h"
#include "space/Player/JTSCharacter.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/World/JTSSpaceWorldManager.h"

UJTSCruiseNavigationWidget::UJTSCruiseNavigationWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), Model(MakeUnique<FJTSNavigationMapModel>()), Frame(MakeUnique<FJTSNavigationMapFrame>()) {}

FString UJTSCruiseNavigationWidget::FormatAstronomicalRange(float Centimeters)
{
	constexpr double AstronomicalUnitCentimeters = 1.495978707e13;
	const double AstronomicalUnits = static_cast<double>(FMath::Max(0.0f, Centimeters)) / AstronomicalUnitCentimeters;
	if (AstronomicalUnits < 0.01)
	{
		const double Kilometers = FMath::Max(0.0f, Centimeters) / 100000.0;
		return Kilometers < 10.0 ? FString::Printf(TEXT("%.1f km"), Kilometers)
			: FString::Printf(TEXT("%.0f km"), Kilometers);
	}
	return FString::Printf(TEXT("%.2f AU"), AstronomicalUnits);
}

float UJTSCruiseNavigationWidget::CruiseRangeRadialFraction(float Centimeters)
{
	const double Kilometers = FMath::Max(0.0, static_cast<double>(Centimeters)) / 100000.0;
	// Fixed logarithmic scope: local moons remain distinguishable alongside AU contacts.
	return static_cast<float>(FMath::Clamp(FMath::Loge(1.0 + Kilometers / 100.0)
		/ FMath::Loge(1.0 + 149597870.7 / 100.0), 0.0, 1.0));
}

FString UJTSCruiseNavigationWidget::FormatNavigationSpeed(float CentimetersPerSecond)
{
	const double SpeedCentimetersPerSecond = FMath::Max(0.0f, CentimetersPerSecond);
	constexpr double AstronomicalUnitCentimeters = 1.495978707e13;
	if (SpeedCentimetersPerSecond >= 0.01 * AstronomicalUnitCentimeters)
	{
		return FString::Printf(TEXT("%.3f AU/s"), SpeedCentimetersPerSecond / AstronomicalUnitCentimeters);
	}
	const double KilometersPerSecond = SpeedCentimetersPerSecond / 100000.0;
	if (KilometersPerSecond >= 1000.0)
	{
		return FString::Printf(TEXT("%.0f km/s"), KilometersPerSecond);
	}
	if (KilometersPerSecond >= 1.0)
	{
		return FString::Printf(TEXT("%.1f km/s"), KilometersPerSecond);
	}
	return FString::Printf(TEXT("%d m/s"), FMath::RoundToInt(SpeedCentimetersPerSecond / 100.0));
}


void UJTSCruiseNavigationWidget::UpdateNavigation(const AJTSSpaceWorldManager* Manager,
    const AJTSSpacecraftActor* Ship, const AJTSCharacter* Character)
{
    *Frame = Model->Build(Manager, Ship, Character);
    if (Frame->bGroundMap && Manager && NavigationSlate.IsValid() && IsVisible())
    {
        if (!SurfaceCapture) SurfaceCapture = NewObject<UJTSSurfaceMapCapture>(this);
        SurfaceCapture->Update(Manager->GetWorld(), *Frame, Manager->NavigationMapSettings, Character, Ship);
        NavigationSlate->SetSurfaceTexture(SurfaceCapture->GetPresentationResource());
    }
    else if (SurfaceCapture) SurfaceCapture->Stop();
    PushPresentation();
}

FString UJTSCruiseNavigationWidget::GetNavigationStage() const
{
    if (Frame->bGroundMap) return TEXT("Ground");
    switch (Frame->Stage)
    {
    case EJTSNavigationMapStage::Surface: return TEXT("Surface");
    case EJTSNavigationMapStage::System: return TEXT("System");
    default: return TEXT("Interstellar");
    }
}

TArray<FString> UJTSCruiseNavigationWidget::GetNavigationContactNames() const
{
    TArray<FString> Names;
    if (Frame->bGroundMap) Names.Add(Frame->PlanetName);
    else if (Frame->Stage == EJTSNavigationMapStage::Interstellar)
        for (const auto& System : Frame->Systems) Names.Add(System.Name);
    else for (const auto& Body : Frame->Bodies) if (Body.bCharted) Names.Add(Body.Name);
    return Names;
}

float UJTSCruiseNavigationWidget::GetNavigationZoom() const
{
    return NavigationSlate.IsValid() ? NavigationSlate->GetZoom() : static_cast<float>(Frame->Stage);
}

UTextureRenderTarget2D* UJTSCruiseNavigationWidget::GetSurfaceMapTexture() const
{
    return SurfaceCapture ? SurfaceCapture->GetTexture() : nullptr;
}

TArray<FVector> UJTSCruiseNavigationWidget::GetNavigationBodyProjections() const
{
    TArray<FVector> Positions;
    for (const auto& Body : Frame->Bodies) if (Body.bCharted)
        Positions.Add(FVector(Body.Position.X, Body.Position.Y, Body.Height));
    return Positions;
}

TSharedRef<SWidget> UJTSCruiseNavigationWidget::RebuildWidget()
{
    NavigationSlate = SNew(SJTSCruiseNavigation);
    if (SurfaceCapture) NavigationSlate->SetSurfaceTexture(SurfaceCapture->GetPresentationResource());
    return NavigationSlate.ToSharedRef();
}

void UJTSCruiseNavigationWidget::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    NavigationSlate.Reset();
    if (SurfaceCapture) SurfaceCapture->Stop();
}

void UJTSCruiseNavigationWidget::PushPresentation()
{
    if (NavigationSlate.IsValid()) NavigationSlate->SetPresentation(*Frame);
}
