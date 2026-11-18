#include "space/UI/JTSExpeditionOverlay.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "space/Core/JTSGameState.h"
#include "space/Components/JTSSpacecraftDurabilityComponent.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "space/World/JTSSurfaceWaypoint.h"
#include "space/World/JTSPlanetAnchor.h"
#include "space/World/JTSSpaceEncounterUnit.h"
#include "space/World/JTSBossCoreDecoy.h"
#include "space/Components/JTSInteriorStateComponent.h"
#include "space/Components/JTSCreatureTacticsComponent.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSHealthComponent.h"
#include "space/Characters/JTSPlanetSurfaceCreature.h"
#include "space/World/JTSFacilityController.h"
#include "space/World/JTSFacilityHazard.h"
#include "Components/BoxComponent.h"
int32 FJTSExpeditionOverlay::Paint(const UUserWidget* W,const FGeometry& G,FSlateWindowElementList& E,int32 L)
{
    auto* PC=W->GetOwningPlayer(); if (!PC || !PC->GetPawn()) return L;
    const auto* GS=W->GetWorld()->GetGameState<AJTSGameState>(); const auto* Ship=GS?GS->GetActiveSpacecraft():nullptr;
    const FVector2D Size=G.GetLocalSize();
    auto Text=[&](const FString& S,FVector2D P,FLinearColor Color=FLinearColor::White)
    { FSlateDrawElement::MakeText(E,L+1,G.ToPaintGeometry(FVector2D(500,30),FSlateLayoutTransform(P)),S,FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),16),ESlateDrawEffect::None,Color); };
    auto Point=[&](FVector P,FVector2D& Out) { return UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC,P,Out,true); };
    if (Ship)
        if (const auto* H=Ship->FindComponentByClass<UJTSSpacecraftDurabilityComponent>())
        {
            const float Fraction=FMath::Clamp(H->Hull/FMath::Max(1.f,H->MaximumHull),0.f,1.f);
            const FVector2D P(Size.X*.5-140,38);
            FSlateDrawElement::MakeBox(E,L,G.ToPaintGeometry(FVector2D(280,8),FSlateLayoutTransform(P)),FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),ESlateDrawEffect::None,FLinearColor(.08f,.09f,.1f,.85f));
            FSlateDrawElement::MakeBox(E,L+1,G.ToPaintGeometry(FVector2D(280*Fraction,8),FSlateLayoutTransform(P)),FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),ESlateDrawEffect::None,Fraction<.3f?FLinearColor(1,.2f,.1f):FLinearColor(.4f,.8f,.75f));
            Text(H->bDisabled?(PC->GetPawn()==Ship?TEXT("飞船失能 · 按 R 重构"):TEXT("飞船失能 · 靠近船体按 E 重构")):FString::Printf(TEXT("船体 %.0f / %.0f"),H->Hull,H->MaximumHull),P+FVector2D(0,12));
            if (!H->bDisabled && H->EstimatedSurvivalSeconds>0) Text(FString::Printf(TEXT("持续受击：预计 %.0f 秒失能"),H->EstimatedSurvivalSeconds),P+FVector2D(0,34),FLinearColor(1,.5f,.2f));
        }
    const auto* Inside=PC->GetPawn()->FindComponentByClass<UJTSInteriorStateComponent>();
    int32 ProgressRow=0;
    for(TActorIterator<AJTSFacilityController> It(W->GetWorld());It;++It)
        if(It->bShowProgressOnHUD && (!Inside || !Inside->IsIndoors()) && FVector::DistSquared(It->GetActorLocation(),PC->GetPawn()->GetActorLocation())<FMath::Square(15000.f))
        {
            const bool bReady=GS && GS->FacilityFlagIds.Contains(It->FollowupFlag);
            Text(It->DisplayName.ToString()+TEXT(" · ")+(bReady?TEXT("发电中"):It->bActive?TEXT("已修复 · 等待尘卷清板"):TEXT("待修复")),FVector2D(28,150+ProgressRow++*28),bReady?FLinearColor(.4f,1,.7f):FLinearColor(1,.75f,.35f));
        }
    for(TActorIterator<AJTSPlanetSurfaceCreature> It(W->GetWorld());It;++It)
    {
        const auto* T=It->FindComponentByClass<UJTSCreatureTacticsComponent>();
        const auto* Health=It->FindComponentByClass<UJTSHealthComponent>();
        if(!T || !Health || Health->IsDead() || FVector::DistSquared(It->GetActorLocation(),PC->GetPawn()->GetActorLocation())>FMath::Square(1400.f)) continue;
        if(T->Posture==EJTSCreaturePosture::Warning)
        {
            TArray<FVector2D> Ring;
            for(int32 I=0;I<=24;++I) { FVector2D P; if(Point(It->GetActorLocation()+It->GetActorUpVector()*8+(It->GetActorForwardVector()*FMath::Cos(I*2*PI/24)+It->GetActorRightVector()*FMath::Sin(I*2*PI/24))*180,P)) Ring.Add(P); }
            if(Ring.Num()>2) FSlateDrawElement::MakeLines(E,L+1,G.ToPaintGeometry(),Ring,ESlateDrawEffect::None,FLinearColor(.85f,.5f,.2f,.85f),true,3);
        }
        const auto* Inventory=PC->GetPawn()->FindComponentByClass<UJTSInventoryComponent>();
        if(T->Tactic==EJTSCreatureTactic::LightFlock && !T->bPassive && Inventory && Inventory->IsWaistLampEquipped() && FVector::DistSquared(It->GetActorLocation(),PC->GetPawn()->GetActorLocation())<FMath::Square(220.f))
        {
            const float Time=W->GetWorld()->GetTimeSeconds();
            for(int32 I=0;I<24;++I)
            {
                const FVector2D P=Size*.5+FVector2D(FMath::Sin(Time*3+I*2.4f)*Size.X*.38,FMath::Cos(Time*4+I*1.7f)*Size.Y*.35);
                FSlateDrawElement::MakeBox(E,L+1,G.ToPaintGeometry(FVector2D(24+I%3*8,9),FSlateLayoutTransform(P)),FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),ESlateDrawEffect::None,FLinearColor(.17f,.14f,.1f,.75f));
            }
            Text(TEXT("绒蛾遮挡 · 关掉腰灯"),FVector2D(Size.X*.5-100,Size.Y*.65),FLinearColor(1,.8f,.5f));
            break;
        }
    }
    for(TActorIterator<AJTSFacilityHazard> It(W->GetWorld());It;++It)
    {
        const FVector Local=It->GetActorTransform().InverseTransformPosition(PC->GetPawn()->GetActorLocation()).GetAbs();
        const FVector Extent=It->Bounds->GetUnscaledBoxExtent();
        if(Local.X>Extent.X || Local.Y>Extent.Y || Local.Z>Extent.Z) continue;
        if(It->bWarning && It->Kind!=EJTSFacilityHazardKind::DustSweep) Text(It->Kind==EJTSFacilityHazardKind::IceLoad?TEXT("冰面开裂 · 立即分散"):TEXT("落冰预警 · 离开此处"),FVector2D(Size.X*.5-110,Size.Y*.6),FLinearColor(1,.5f,.2f));
        if(It->Kind==EJTSFacilityHazardKind::SporeFog)
        {
            FSlateDrawElement::MakeBox(E,L,G.ToPaintGeometry(Size,FSlateLayoutTransform()),FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),ESlateDrawEffect::None,FLinearColor(.65f,.62f,.48f,.12f));
            const auto* Inventory=PC->GetPawn()->FindComponentByClass<UJTSInventoryComponent>();
            if(Inventory && !Inventory->IsWaistLampEquipped()) Text(TEXT("孢子雾 · 开启腰灯"),FVector2D(Size.X*.5-100,Size.Y*.6),FLinearColor(1,.8f,.5f));
        }
    }
    if (!Inside || !Inside->IsIndoors())
        for (TActorIterator<AJTSSurfaceWaypoint> It(W->GetWorld());It;++It)
        {
            if (!It->IsDiscovered() || !It->Planet || FVector::Dist(It->GetActorLocation(),PC->GetPawn()->GetActorLocation())>It->Planet->GetApproximateRadius()*5) continue;
            FVector2D P; const bool bVisible=Point(It->GetActorLocation(),P);
            if (!bVisible) { const FVector D=It->GetActorLocation()-PC->GetPawn()->GetActorLocation(); P=Size*.5+FVector2D(FVector::DotProduct(D,PC->GetControlRotation().Quaternion().GetRightVector()),-FVector::DotProduct(D,PC->GetControlRotation().Quaternion().GetUpVector())).GetSafeNormal()*Size.Y*.48; }
            P.X=FMath::Clamp(P.X,35.,Size.X-260); P.Y=FMath::Clamp(P.Y,90.,Size.Y-80);
            Text(FString::Printf(TEXT("◆ %s · %.0f m"),*It->DisplayName.ToString(),FVector::Dist(It->GetActorLocation(),PC->GetPawn()->GetActorLocation())/100),P,FLinearColor(1,.8f,.4f));
        }
    struct FTeam { FBox2D Box=FBox2D(ForceInit); bool bRay=false,bHostile=false; };
    TMap<int32,FTeam> Teams;
    for (TActorIterator<AJTSSpaceEncounterUnit> It(W->GetWorld());It;++It)
    {
        FVector2D P; if (!Point(It->GetActorLocation(),P) || It->bIsSalvage) continue;
        auto& T=Teams.FindOrAdd(It->TeamId); T.Box+=P; T.bRay=It->bLightSailRay; T.bHostile|=It->bHostile;
        if (It->bHeating && Ship) { FVector2D Heat; if (Point(It->HeatPoint,Heat)) Text(TEXT("◎ 热斑 · 改变航向"),Heat,FLinearColor(1,.3f,.1f)); }
    }
    for (const auto& Pair:Teams)
    {
        const auto& T=Pair.Value; const FVector2D Center=T.Box.GetCenter(); const double R=FMath::Max(Size.Y*.02,T.Box.GetExtent().Size()+12);
        TArray<FVector2D> Ring; for(int I=0;I<=32;++I) Ring.Add(Center+FVector2D(FMath::Cos(I*2*PI/32),FMath::Sin(I*2*PI/32))*R);
        FLinearColor Color=T.bRay?FLinearColor(1,.7f,.25f):FLinearColor(.8f,.85f,.9f); Color.A=T.bHostile?.5f+.5f*FMath::Sin(W->GetWorld()->GetTimeSeconds()*5):.45f;
        FSlateDrawElement::MakeLines(E,L+1,G.ToPaintGeometry(),Ring,ESlateDrawEffect::None,Color,true,2);
    }
    for (TActorIterator<AJTSBossCoreDecoy> It(W->GetWorld());It;++It) if (It->IsNameplateLit(PC->GetPawn())) { FVector2D P; if(Point(It->GetActorLocation(),P)) Text(TEXT("✦ ORIGINAL FABRICATOR · UNIT 000"),P,FLinearColor(1,1,.6f)); }
    return L+2;
}
