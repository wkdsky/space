#include "JTSMirrorGlareWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "space/Components/JTSMirrorDefenseComponent.h"
#include "space/Systems/JTSMirrorGlareSubsystem.h"

void UJTSMirrorGlareWidget::InitializeMaterial(UMaterialInterface* Material)
{
	Glare = UMaterialInstanceDynamic::Create(Material, this);
	auto* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Image->SetBrushFromMaterial(Glare);
	WidgetTree->RootWidget = Image;
	SetVisibility(ESlateVisibility::HitTestInvisible);
	// Stable regions, permuted once per viewer; a new source takes one free region without flickering.
	int32 Order[3] = {0,1,2};
	for (int32 I = 2; I > 0; --I) Swap(Order[I], Order[FMath::RandRange(0,I)]);
	for (int32 I = 0; I < 3; ++I)
		Centers[I] = FVector2D(.18f + Order[I]*.32f + FMath::FRandRange(-.06f,.06f), FMath::FRandRange(.28f,.72f));
}

void UJTSMirrorGlareWidget::NativeTick(const FGeometry& Geometry, float Dt)
{
	Super::NativeTick(Geometry, Dt);
	if (!Glare || !GetOwningPlayer()) return;
	ScanTime -= Dt;
	if (ScanTime <= 0)
	{
		ScanTime = .06f;
		TArray<UJTSMirrorDefenseComponent*> Visible;
		if (auto* System = GetWorld()->GetSubsystem<UJTSMirrorGlareSubsystem>())
			for (auto Source : System->GetSources())
				if (Source.IsValid() && Source->CanGlareAt(GetOwningPlayer())) Visible.Add(Source.Get());
		IlluminatingCount = Visible.Num();
		for (int32 I = 0; I < 3; ++I)
		{
			Illuminated[I] = Assigned[I].IsValid() && Visible.Contains(Assigned[I].Get());
			if (Illuminated[I]) Visible.Remove(Assigned[I].Get());
		}
		for (int32 I = 0; I < 3; ++I)
			if (!Illuminated[I] && !Visible.IsEmpty() && Strength[I] < .02f)
			{
				Assigned[I] = Visible.Pop(); Illuminated[I] = true;
			}
	}
	for (int32 I = 0; I < 3; ++I)
	{
		Strength[I] = FMath::FInterpConstantTo(Strength[I], Illuminated[I] ? 1.f : 0.f, Dt, Illuminated[I] ? 2.5f : 1.1f);
		const FName Name(*FString::Printf(TEXT("Spot%d"),I));
		Glare->SetVectorParameterValue(Name, FLinearColor(Centers[I].X, Centers[I].Y, Strength[I], 0));
	}
	const float MinimumStrength = FMath::Min3(Strength[0],Strength[1],Strength[2]);
	const float DesiredFull = FullOcclusionForCount(IlluminatingCount) * MinimumStrength;
	FullOcclusion = FMath::FInterpConstantTo(FullOcclusion, DesiredFull, Dt, DesiredFull > FullOcclusion ? 1.8f : 1.4f);
	Glare->SetScalarParameterValue(TEXT("FullWhite"), FullOcclusion);
	const FVector2D Size = Geometry.GetLocalSize();
	Glare->SetScalarParameterValue(TEXT("Aspect"), Size.Y > 0 ? Size.X/Size.Y : 1.77778f);
}
