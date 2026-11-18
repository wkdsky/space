#include "space/World/JTSFacilityPowerReveal.h"
#include "space/World/JTSFacilityController.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Components/JTSHealthComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RotationMatrix.h"
#include "Engine/World.h"
#include "TimerManager.h"

AJTSFacilityPowerReveal::AJTSFacilityPowerReveal()
{
    bReplicates=true; PrimaryActorTick.bCanEverTick=false;
    SceneRoot=CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")); SetRootComponent(SceneRoot);
    Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("Camera")); Camera->SetupAttachment(SceneRoot);
    Camera->SetFieldOfView(54);
}
void AJTSFacilityPowerReveal::BeginPlay()
{
    Super::BeginPlay();
    if (Source) Source->OnRuntimeActivated.AddDynamic(this,&ThisClass::OnPowerChanged);
    GetWorldTimerManager().SetTimerForNextTick([Weak=TWeakObjectPtr<AJTSFacilityPowerReveal>(this)](){ if (Weak.IsValid()) Weak->bInitialStateReceived=true; });
}
void AJTSFacilityPowerReveal::OnPowerChanged(bool bActive)
{
    if (HasAuthority() && bInitialStateReceived && bActive) MulticastReveal();
}
void AJTSFacilityPowerReveal::MulticastReveal_Implementation()
{
    if (GetNetMode()==NM_DedicatedServer || LocalController.IsValid()) return;
    auto* PC=Cast<AJTSPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    auto* Pawn=PC?Cast<AJTSCharacter>(PC->GetPawn()):nullptr;
    if (!Pawn || !PC->IsLocalController() || Pawn->IsBoarded() || Pawn->GetHealthComponent()->IsDead()
        || FVector::DistSquared(Pawn->GetActorLocation(),GetActorLocation())>FMath::Square(AudienceRadius)) return;
    PC->ClientShowExpeditionNotice(Notice);
    if (PowerOnSound) UGameplayStatics::PlaySound2D(this,PowerOnSound);
    LocalController=PC; PreviousView=PC->GetViewTarget(); OriginalPawn=Pawn;
    Started=GetWorld()->GetTimeSeconds();
    Camera->SetRelativeLocation(CameraStart);
    const FVector Position=GetActorTransform().TransformPosition(CameraStart);
    const FVector Target=GetActorTransform().TransformPosition(Focus);
    Camera->SetWorldRotation(FRotationMatrix::MakeFromXZ(Target-Position,GetActorUpVector()).Rotator());
    PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true);
    PC->SetViewTargetWithBlend(this,.55f);
    GetWorldTimerManager().SetTimer(CameraTimer,this,&ThisClass::Advance,.02f,true);
}
void AJTSFacilityPowerReveal::Advance()
{
    auto* PC=LocalController.Get();
    auto* Pawn=Cast<AJTSCharacter>(OriginalPawn.Get());
    if (!PC || !Pawn || PC->GetPawn()!=Pawn || Pawn->IsBoarded() || Pawn->GetHealthComponent()->IsDead()) { Restore(); return; }
    const float T=FMath::Clamp(float(GetWorld()->GetTimeSeconds()-Started)/FMath::Max(1.f,Duration),0.f,1.f);
    const float Smooth=FMath::SmoothStep(0.f,1.f,T);
    Camera->SetRelativeLocation(FMath::Lerp(CameraStart,CameraEnd,Smooth));
    Camera->SetWorldRotation(FRotationMatrix::MakeFromXZ(GetActorTransform().TransformPosition(Focus)-Camera->GetComponentLocation(),GetActorUpVector()).Rotator());
    Camera->SetFieldOfView(FMath::Lerp(54.f,46.f,Smooth));
    if (T>=1) Restore();
}
void AJTSFacilityPowerReveal::Restore()
{
    GetWorldTimerManager().ClearTimer(CameraTimer);
    if (auto* PC=LocalController.Get())
    {
        if (PC->GetViewTarget()==this) PC->SetViewTargetWithBlend(PC->GetPawn()!=OriginalPawn.Get()?PC->GetPawn():PreviousView.Get(),.6f);
        PC->SetIgnoreMoveInput(false); PC->SetIgnoreLookInput(false);
    }
    LocalController.Reset(); PreviousView.Reset(); OriginalPawn.Reset();
}
void AJTSFacilityPowerReveal::EndPlay(EEndPlayReason::Type Reason)
{
    Restore();
    if (Source) Source->OnRuntimeActivated.RemoveDynamic(this,&ThisClass::OnPowerChanged);
    Super::EndPlay(Reason);
}
