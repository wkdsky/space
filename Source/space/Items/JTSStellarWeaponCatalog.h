#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "JTSStellarWeaponCatalog.generated.h"

class UJTSStellarLootTable;
class AJTSStellarEffectActor;
class UStaticMesh;

UENUM(BlueprintType)
enum class EJTSStellarWeaponMode : uint8 { Jet, Focus, BlackHole, PresentationOnly };

USTRUCT(BlueprintType)
struct SPACE_API FJTSStellarWeaponDefinition
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName CoreId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName AttachmentId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EJTSStellarWeaponMode Mode = EJTSStellarWeaponMode::Jet;
	/** Jet/black hole: DPS. Focus: damage per shot. Independent of enemy health. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float BaseDamage = 300.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float StatusDamagePerSecond = 40.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) float RangeCentimeters = 600.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.01")) float EnergyPerSecond = 22.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1", ClampMax="256")) int32 MaximumTargets = 96;
	/** Full cone angle, not the angle on either side of the centre ray. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Area", meta=(ClampMin="5", ClampMax="160")) float ConeAngleDegrees = 120.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Jet|Secondary", meta=(ClampMin="1")) float FocusedRangeCentimeters = 2200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Jet|Secondary", meta=(ClampMin="5", ClampMax="120")) float FocusedConeAngleDegrees = 30.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Area", meta=(ClampMin="0")) float RangePerLevel = 30.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Area", meta=(ClampMin="1")) float AreaRadiusCentimeters = 400.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Area", meta=(ClampMin="0")) float RadiusPerLevel = 25.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Area", meta=(ClampMin="1")) float RepulsionRadiusCentimeters = 300.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Area", meta=(ClampMin="0", ClampMax="600")) float PullSpeed = 260.0f;
	/** A black hole is paid for once and continues independently of the selected weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BlackHole", meta=(ClampMin="0.01", ClampMax="100")) float BlackHoleEnergyPerCast = 30.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BlackHole", meta=(ClampMin="0.2", ClampMax="60")) float BlackHoleLifetimeSeconds = 8.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BlackHole", meta=(ClampMin="0.1")) float BlackHoleCastCooldown = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BlackHole", meta=(ClampMin="1", ClampMax="6")) int32 BlackHoleBaseMaximumFields = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BlackHole", meta=(ClampMin="10")) float BlackHoleCoreRadiusCentimeters = 65.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BlackHole", meta=(ClampMin="0")) float BlackHoleGroundClearanceCentimeters = 45.0f;
	/** Spring force per centimetre inside the player-centred repulsion boundary (s^-2). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BlackHole", meta=(ClampMin="1")) float RepulsionStiffness = 180.0f;
	/** Shallow penetration before the barrier catches an approaching enemy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BlackHole", meta=(ClampMin="0")) float RepulsionEntryDepthCentimeters = 30.0f;
	/** Damped resting position just outside the visible dome. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BlackHole", meta=(ClampMin="0", ClampMax="50")) float RepulsionExitOffsetCentimeters = 12.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beam", meta=(ClampMin="0")) float BeamRadiusCentimeters = 0.0f;
	/** Total targets on the direct ray, including its first target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beam", meta=(ClampMin="1", ClampMax="16")) int32 BasePierceTargets = 2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beam", meta=(ClampMin="0", ClampMax="8")) int32 BaseChainTargets = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beam", meta=(ClampMin="0")) float ChainRadiusCentimeters = 500.0f;
	/** Blueprint selects meshes, materials, Niagara and audio. No project assets in C++. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TSubclassOf<AJTSStellarEffectActor> EffectClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor Color = FLinearColor(1, 0.25f, 0.05f);
	/** Presentation assets and item-local pivots are authored in the catalog, never in gameplay code. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation") TSoftObjectPtr<UStaticMesh> HeldMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation") FTransform HeldGripTransform = FTransform::Identity;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation") FTransform HeldMuzzleTransform = FTransform::Identity;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation", meta=(ClampMin="-1")) int32 CoreMaterialSlot = -1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation") bool bMeleePresentation = false;
	/** Model +Z follows the character's local gravity up, independently of aim pitch. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation") bool bUprightScepter = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation") FRotator HeldCarryRotation = FRotator::ZeroRotator;
};

UCLASS(BlueprintType)
class SPACE_API UJTSStellarWeaponCatalog : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UJTSStellarLootTable> LootTable;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FJTSStellarWeaponDefinition> Weapons;
	const FJTSStellarWeaponDefinition* Find(FName Core, FName Attachment) const
	{
		return Weapons.FindByPredicate([&](const FJTSStellarWeaponDefinition& Def)
			{ return Def.CoreId == Core && Def.AttachmentId == Attachment; });
	}
};
