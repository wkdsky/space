#include "space/UI/JTSFloatingDamageActor.h"

#include "Components/WidgetComponent.h"
#include "space/UI/JTSFloatingDamageWidget.h"

AJTSFloatingDamageActor::AJTSFloatingDamageActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	WidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("DamageWidget"));
	SetRootComponent(WidgetComponent);
	WidgetComponent->SetWidgetClass(UJTSFloatingDamageWidget::StaticClass());
	WidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	WidgetComponent->SetDrawSize(FVector2D(200.0f, 80.0f));
	WidgetComponent->SetPivot(FVector2D(0.5f, 0.5f));
	WidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WidgetComponent->SetGenerateOverlapEvents(false);
}

void AJTSFloatingDamageActor::BeginPlay()
{
	Super::BeginPlay();
	WidgetComponent->InitWidget();
	SetLifeSpan(0.92f);
}

void AJTSFloatingDamageActor::Initialize(float Damage, bool bCritical, const FVector& SurfaceUp)
{
	StartLocation = GetActorLocation();
	Up = SurfaceUp.GetSafeNormal();
	if (Up.IsNearlyZero()) Up = FVector::UpVector;
	Side = FVector::CrossProduct(Up, FVector::ForwardVector).GetSafeNormal();
	if (Side.IsNearlyZero()) Side = FVector::CrossProduct(Up, FVector::RightVector).GetSafeNormal();
	Side *= FMath::RandBool() ? 1.0f : -1.0f;
	WidgetComponent->InitWidget();
	TotalDamage = 0.0f;
	AddDamage(Damage, bCritical);
}

void AJTSFloatingDamageActor::AddDamage(float Damage, bool bCritical)
{
	TotalDamage += FMath::Max(0.0f, Damage);
	bIsCritical |= bCritical;
	if (UJTSFloatingDamageWidget* Widget = Cast<UJTSFloatingDamageWidget>(WidgetComponent->GetUserWidgetObject()))
	{
		Widget->SetDamage(TotalDamage, bIsCritical);
	}
}

void AJTSFloatingDamageActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += FMath::Max(0.0f, DeltaSeconds);
	const float T = FMath::Clamp(Age / 0.92f, 0.0f, 1.0f);
	const float Lift = 102.0f * (1.0f - FMath::Square(1.0f - T));
	const float Drift = (bIsCritical ? 42.0f : 27.0f) * T;
	SetActorLocation(StartLocation + Up * Lift + Side * Drift);
	if (UUserWidget* Widget = WidgetComponent->GetUserWidgetObject())
	{
		const float Pop = T < 0.16f ? FMath::Lerp(0.70f, bIsCritical ? 1.30f : 1.15f, T / 0.16f)
			: FMath::Lerp(bIsCritical ? 1.30f : 1.15f, 1.0f, FMath::Clamp((T - 0.16f) / 0.25f, 0.0f, 1.0f));
		Widget->SetRenderScale(FVector2D(Pop, Pop));
		Widget->SetRenderOpacity(T < 0.58f ? 1.0f : 1.0f - (T - 0.58f) / 0.42f);
	}
}
