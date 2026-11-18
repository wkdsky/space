#include "space/World/JTSInteriorZone.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Math/RotationMatrix.h"
#include "space/World/JTSPlanetAnchor.h"
AJTSInteriorZone::AJTSInteriorZone()
{
    bReplicates=true; bAlwaysRelevant=true; PrimaryActorTick.bCanEverTick=false;
    Bounds=CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds")); SetRootComponent(Bounds);
    Bounds->SetBoxExtent(FVector(1000,1000,300)); Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
bool AJTSInteriorZone::Contains(const FVector& P) const
{
    if (GetActorScale3D().GetAbsMin()<=UE_SMALL_NUMBER) return false;
    const FVector Local=GetActorTransform().InverseTransformPosition(P).GetAbs(), Extent=Bounds->GetUnscaledBoxExtent();
    return Local.X<=Extent.X && Local.Y<=Extent.Y && Local.Z<=Extent.Z;
}
AJTSInteriorZone* AJTSInteriorZone::FindAt(const UWorld* W,const FVector& P)
{
    if (W) for (TActorIterator<AJTSInteriorZone> It(W); It; ++It) if (It->Contains(P)) return *It;
    return nullptr;
}
bool AJTSInteriorZone::ProjectFloor(AJTSPlanetAnchor* P,const FVector& Point,FJTSPlanetSurfaceHit& Out)
{
    if (!P) return false;
    auto* Z=FindAt(P->GetWorld(),Point); if (!Z || Z->Planet!=P) return false;
    const FVector Up=P->GetRadialUpVector(Point);
    FHitResult Best; double BestDist=TNumericLimits<double>::Max();
    bool bFound=false;
    for (AActor* A:Z->FloorActors)
    {
        if (!IsValid(A)) continue;
        TInlineComponentArray<UPrimitiveComponent*> Parts(A);
        for (auto* C:Parts)
        {
            if (!C->IsCollisionEnabled()) continue;
            FHitResult H;
            if ((C->LineTraceComponent(H,Point+Up*120,Point-Up*900,FCollisionQueryParams(SCENE_QUERY_STAT(InteriorFloor),false))
                || C->LineTraceComponent(H,Point+Up*120,Point-Up*900,FCollisionQueryParams(SCENE_QUERY_STAT(InteriorFloorComplex),true))) && H.Distance<BestDist)
            { Best=H; BestDist=H.Distance; bFound=true; }
        }
    }
    // Component geometry traces report a contact with an overlap-style query filter in Chaos.
    // Their return value is authoritative; bBlockingHit is only meaningful for a world channel trace.
    if (!bFound) return false;
    Out.bBlockingHit=true; Out.ImpactPoint=Best.ImpactPoint; Out.ImpactNormal=Best.ImpactNormal;
    Out.HitActor=Best.GetActor(); Out.HitComponent=Best.GetComponent(); Out.Distance=Best.Distance;
    return FVector::DotProduct(Out.ImpactNormal,Up)>.5f;
}
bool AJTSInteriorZone::ProjectGameplayGround(AJTSPlanetAnchor* P,const FVector& Point,FJTSPlanetSurfaceHit& Out)
{
    if (!P) return false;
    if (FindAt(P->GetWorld(),Point)) return ProjectFloor(P,Point,Out);
    return P->ProjectPointToSurface(Point,Out);
}
bool AJTSInteriorZone::GroundFrame(AJTSPlanetAnchor* P,const FVector& Point,const FVector& Forward,FJTSPlanetSurfaceFrame& Out)
{
    if (!P) return false;
    if (!FindAt(P->GetWorld(),Point)) return P->GetSurfaceFrameAt(Point,Forward,Out);
    FJTSPlanetSurfaceHit H; if (!ProjectFloor(P,Point,H)) return false;
    Out.Location=H.ImpactPoint; Out.Up=P->GetRadialUpVector(H.ImpactPoint);
    Out.Forward=FVector::VectorPlaneProject(Forward,Out.Up).GetSafeNormal();
    if (Out.Forward.IsNearlyZero()) Out.Up.FindBestAxisVectors(Out.Forward,Out.Right);
    Out.Right=FVector::CrossProduct(Out.Up,Out.Forward).GetSafeNormal();
    Out.Transform=FTransform(FRotationMatrix::MakeFromXZ(Out.Forward,Out.Up).ToQuat(),Out.Location);
    return true;
}
