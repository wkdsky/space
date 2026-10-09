// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/Items/JTSItemDefinitionLibrary.h"

#include "Misc/PackageName.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/StrongObjectPtr.h"
#include "space/Items/JTSItemDefinition.h"

namespace
{
	TMap<EJTSItemId, TObjectPtr<UJTSItemDefinition>> GFallbackDefinitions;
	// This cache is outside a UObject, so TObjectPtr alone does not keep loaded assets alive for GC.
	TMap<EJTSItemId, TStrongObjectPtr<UJTSItemDefinition>> GLoadedDefinitions;

	int32 CapabilityMask(std::initializer_list<EJTSItemCapability> Capabilities)
	{
		int32 Result = 0;
		for (const EJTSItemCapability Capability : Capabilities)
		{
			Result |= static_cast<int32>(Capability);
		}
		return Result;
	}

	FJTSItemCost Cost(EJTSResourceType Type, int32 Amount)
	{
		FJTSItemCost Result;
		Result.ResourceType = Type;
		Result.Amount = Amount;
		return Result;
	}

	FString AssetPathFor(EJTSItemId ItemId)
	{
		switch (ItemId)
		{
		case EJTSItemId::ShortBlade: return TEXT("/Game/Space/Data/Items/DA_Item_ShortBlade.DA_Item_ShortBlade");
		case EJTSItemId::ShockPole: return TEXT("/Game/Space/Data/Items/DA_Item_ShockPole.DA_Item_ShockPole");
		case EJTSItemId::PowerHammer: return TEXT("/Game/Space/Data/Items/DA_Item_PowerHammer.DA_Item_PowerHammer");
		case EJTSItemId::RailPistol: return TEXT("/Game/Space/Data/Items/DA_Item_RailPistol.DA_Item_RailPistol");
		case EJTSItemId::AssaultRifle: return TEXT("/Game/Space/Data/Items/DA_Item_AssaultRifle.DA_Item_AssaultRifle");
		case EJTSItemId::Shotgun: return TEXT("/Game/Space/Data/Items/DA_Item_Shotgun.DA_Item_Shotgun");
		case EJTSItemId::RailSniper: return TEXT("/Game/Space/Data/Items/DA_Item_RailSniper.DA_Item_RailSniper");
		case EJTSItemId::HeavyMachineGun: return TEXT("/Game/Space/Data/Items/DA_Item_HeavyMachineGun.DA_Item_HeavyMachineGun");
		case EJTSItemId::GrenadeLauncher: return TEXT("/Game/Space/Data/Items/DA_Item_GrenadeLauncher.DA_Item_GrenadeLauncher");
		case EJTSItemId::ArcGun: return TEXT("/Game/Space/Data/Items/DA_Item_ArcGun.DA_Item_ArcGun");
		case EJTSItemId::Rock: return TEXT("/Game/Space/Data/Items/DA_Item_Rock.DA_Item_Rock");
		case EJTSItemId::Ore: return TEXT("/Game/Space/Data/Items/DA_Item_Ore.DA_Item_Ore");
		case EJTSItemId::Fuel: return TEXT("/Game/Space/Data/Items/DA_Item_Fuel.DA_Item_Fuel");
		case EJTSItemId::Water: return TEXT("/Game/Space/Data/Items/DA_Item_Water.DA_Item_Water");
		case EJTSItemId::Food: return TEXT("/Game/Space/Data/Items/DA_Item_Food.DA_Item_Food");
		case EJTSItemId::MoonAntCorpse: return TEXT("/Game/Space/Data/Items/DA_Item_MoonAntCorpse.DA_Item_MoonAntCorpse");
		case EJTSItemId::WaistLamp: return TEXT("/Game/Space/Data/Items/DA_Item_WaistLamp.DA_Item_WaistLamp");
		default: return FString();
		}
	}

	/** Code-side baseline for each normal weapon. A Data Asset with the same ItemId overrides every value. */
	struct FWeaponSeed
	{
		EJTSItemId Id = EJTSItemId::None;
		const TCHAR* Name = TEXT("");
		const TCHAR* Description = TEXT("");
		bool bRanged = false;
		EJTSMeleeAttackType MeleeClass = EJTSMeleeAttackType::Improvised;
		float CombatDamage = 1.0f;
		float MeleeInterval = 0.45f;
		float MiningWork = 0.0f;
		float RangedDamage = 0.0f;
		float RangedInterval = 0.35f;
		float RangedRange = 9000.0f;
		float AimFOV = 60.0f;
		float HipSpread = 1.2f;
		float AimSpread = 0.15f;
		float ViewKick = 0.8f;
		const TCHAR* FireSound = TEXT("/Game/Space/Audio/S_JTSPistolFire.S_JTSPistolFire");
		int32 Rock = 0;
		int32 Ore = 0;
		int32 Organic = 0;
		float UpgradeCostMultiplier = 1.0f;
		FLinearColor Accent = FLinearColor(0.38f, 0.70f, 0.95f, 1.0f);
		// Held silhouette: body, barrel, sight (scale + location), grip and muzzle location, grip scale.
		FVector BodyScale = FVector::OneVector;
		FVector BodyLoc = FVector::ZeroVector;
		FVector BarrelScale = FVector::OneVector;
		FVector BarrelLoc = FVector::ZeroVector;
		FVector SightScale = FVector::ZeroVector;
		FVector SightLoc = FVector::ZeroVector;
		FVector GripLoc = FVector::ZeroVector;
		FVector MuzzleLoc = FVector::ZeroVector;
		FVector GripScale = FVector(0.16f, 0.12f, 0.32f);
	};

	void SetSilhouette(FWeaponSeed& S, FVector Body, FVector BodyAt, FVector Barrel, FVector BarrelAt,
		FVector Sight, FVector SightAt, FVector GripAt, FVector MuzzleAt, FVector Grip)
	{
		S.BodyScale = Body; S.BodyLoc = BodyAt; S.BarrelScale = Barrel; S.BarrelLoc = BarrelAt;
		S.SightScale = Sight; S.SightLoc = SightAt; S.GripLoc = GripAt; S.MuzzleLoc = MuzzleAt; S.GripScale = Grip;
	}

	const TArray<FWeaponSeed>& GetWeaponSeeds()
	{
		// Damage is on a 100-health baseline; feel values (FOV, spread, kick) carry over from the prototype weapons.
		// Mining work is per hit: the hammer replaces the old pickaxe as the dedicated miner.
		static const TArray<FWeaponSeed> Seeds = []()
		{
			TArray<FWeaponSeed> Result;
			FWeaponSeed S;

			S = FWeaponSeed();
			S.Id = EJTSItemId::ShortBlade; S.Name = TEXT("合金短刃"); S.Description = TEXT("快速连刺的近战短刃。开荒省资源，也能慢慢凿开矿石。");
			S.MeleeClass = EJTSMeleeAttackType::Knife; S.CombatDamage = 35.0f; S.MeleeInterval = 0.417f; S.MiningWork = 0.75f;
			S.Rock = 3; S.Ore = 1; S.UpgradeCostMultiplier = 0.6f; S.Accent = FLinearColor(0.82f, 0.89f, 1.0f);
			SetSilhouette(S, FVector(0.30f, 0.09f, 0.10f), FVector(4, 0, 0), FVector(0.46f, 0.06f, 0.05f), FVector(42, 0, 0),
				FVector::ZeroVector, FVector::ZeroVector, FVector(0, 0, 0), FVector(65, 0, 0), FVector(0.22f, 0.12f, 0.12f));
			Result.Add(S);

			S = FWeaponSeed();
			S.Id = EJTSItemId::ShockPole; S.Name = TEXT("震荡长柄"); S.Description = TEXT("长柄震荡武器，扇形横扫，适合近战清群。");
			S.MeleeClass = EJTSMeleeAttackType::Axe; S.CombatDamage = 55.0f; S.MeleeInterval = 0.714f; S.MiningWork = 2.5f;
			S.Rock = 6; S.Ore = 4; S.UpgradeCostMultiplier = 0.8f; S.Accent = FLinearColor(0.20f, 0.85f, 0.90f);
			SetSilhouette(S, FVector(1.10f, 0.06f, 0.06f), FVector(10, 0, 0), FVector(0.16f, 0.14f, 0.14f), FVector(70, 0, 0),
				FVector::ZeroVector, FVector::ZeroVector, FVector(0, 0, 0), FVector(84, 0, 0), FVector(0.26f, 0.12f, 0.14f));
			Result.Add(S);

			S = FWeaponSeed();
			S.Id = EJTSItemId::PowerHammer; S.Name = TEXT("动力战锤"); S.Description = TEXT("沉重的动力锤，一击破甲，同时是最好的采矿工具。");
			S.MeleeClass = EJTSMeleeAttackType::Tool; S.CombatDamage = 120.0f; S.MeleeInterval = 1.54f; S.MiningWork = 10.0f;
			S.Rock = 8; S.Ore = 4; S.UpgradeCostMultiplier = 1.0f; S.Accent = FLinearColor(1.0f, 0.70f, 0.18f);
			SetSilhouette(S, FVector(0.90f, 0.07f, 0.07f), FVector(8, 0, 0), FVector(0.34f, 0.30f, 0.30f), FVector(56, 0, 0),
				FVector::ZeroVector, FVector::ZeroVector, FVector(-6, 0, 0), FVector(72, 0, 0), FVector(0.26f, 0.12f, 0.14f));
			Result.Add(S);

			S = FWeaponSeed();
			S.Id = EJTSItemId::RailPistol; S.Name = TEXT("磁轨手枪"); S.Description = TEXT("精准的半自动手枪，入门首选，对矿石只有微弱的凿击。");
			S.bRanged = true; S.MiningWork = 0.4f; S.RangedDamage = 42.0f; S.RangedInterval = 0.40f; S.RangedRange = 9000.0f;
			S.AimFOV = 68.0f; S.HipSpread = 1.1f; S.AimSpread = 0.12f; S.ViewKick = 0.9f;
			S.Rock = 2; S.Ore = 4; S.UpgradeCostMultiplier = 0.6f; S.Accent = FLinearColor(0.42f, 0.72f, 1.0f);
			SetSilhouette(S, FVector(0.48f, 0.18f, 0.14f), FVector(13, 0, 0), FVector(0.26f, 0.08f, 0.08f), FVector(48, 0, 0),
				FVector(0.08f, 0.06f, 0.10f), FVector(19, 0, 12), FVector(0, 0, -8), FVector(61, 0, 0), FVector(0.16f, 0.12f, 0.32f));
			Result.Add(S);

			S = FWeaponSeed();
			S.Id = EJTSItemId::AssaultRifle; S.FireSound = TEXT("/Game/Space/Audio/S_JTSMachineGunFire.S_JTSMachineGunFire"); S.Name = TEXT("突击步枪"); S.Description = TEXT("通用主力步枪，持续射击时散布逐渐展开。");
			S.bRanged = true; S.MiningWork = 0.3f; S.RangedDamage = 18.0f; S.RangedInterval = 0.1667f; S.RangedRange = 9000.0f;
			S.AimFOV = 62.0f; S.HipSpread = 2.0f; S.AimSpread = 0.35f; S.ViewKick = 0.5f;
			S.Rock = 8; S.Ore = 12; S.UpgradeCostMultiplier = 1.0f; S.Accent = FLinearColor(0.55f, 0.85f, 0.45f);
			SetSilhouette(S, FVector(0.80f, 0.20f, 0.16f), FVector(16, 0, 0), FVector(0.34f, 0.08f, 0.08f), FVector(66, 0, 0),
				FVector(0.10f, 0.06f, 0.10f), FVector(26, 0, 13), FVector(0, 0, -10), FVector(84, 0, 0), FVector(0.20f, 0.14f, 0.36f));
			Result.Add(S);

			S = FWeaponSeed();
			S.Id = EJTSItemId::Shotgun; S.FireSound = TEXT("/Game/Space/Audio/S_JTSSniperFire.S_JTSSniperFire"); S.Name = TEXT("霰弹枪"); S.Description = TEXT("近距离清群的霰弹枪，远处伤害明显衰减。");
			S.bRanged = true; S.MiningWork = 1.2f; S.RangedDamage = 95.0f; S.RangedInterval = 0.909f; S.RangedRange = 3000.0f;
			S.AimFOV = 64.0f; S.HipSpread = 3.5f; S.AimSpread = 2.2f; S.ViewKick = 2.4f;
			S.Rock = 8; S.Ore = 10; S.UpgradeCostMultiplier = 1.0f; S.Accent = FLinearColor(0.90f, 0.55f, 0.20f);
			SetSilhouette(S, FVector(0.84f, 0.22f, 0.16f), FVector(17, 0, 0), FVector(0.52f, 0.12f, 0.12f), FVector(72, 0, 2),
				FVector(0.06f, 0.05f, 0.07f), FVector(92, 0, 9), FVector(0, 0, -10), FVector(100, 0, 2), FVector(0.22f, 0.15f, 0.38f));
			Result.Add(S);

			S = FWeaponSeed();
			S.Id = EJTSItemId::RailSniper; S.FireSound = TEXT("/Game/Space/Audio/S_JTSSniperFire.S_JTSSniperFire"); S.Name = TEXT("电磁狙击枪"); S.Description = TEXT("远程精英与弱点的解法，被包围时很危险。");
			S.bRanged = true; S.MiningWork = 1.0f; S.RangedDamage = 150.0f; S.RangedInterval = 1.54f; S.RangedRange = 16000.0f;
			S.AimFOV = 36.0f; S.HipSpread = 2.5f; S.AimSpread = 0.04f; S.ViewKick = 2.1f;
			S.Rock = 10; S.Ore = 18; S.UpgradeCostMultiplier = 1.2f; S.Accent = FLinearColor(0.72f, 0.42f, 1.0f);
			SetSilhouette(S, FVector(1.10f, 0.20f, 0.14f), FVector(23, 0, 0), FVector(0.62f, 0.08f, 0.08f), FVector(96, 0, 0),
				FVector(0.12f, 0.09f, 0.18f), FVector(30, 0, 17), FVector(0, 0, -10), FVector(127, 0, 0), FVector(0.22f, 0.15f, 0.42f));
			Result.Add(S);

			S = FWeaponSeed();
			S.Id = EJTSItemId::HeavyMachineGun; S.FireSound = TEXT("/Game/Space/Audio/S_JTSMachineGunFire.S_JTSMachineGunFire"); S.Name = TEXT("重机枪"); S.Description = TEXT("守点与割草的重机枪，射速极高，持续射击会过热。");
			S.bRanged = true; S.MiningWork = 0.3f; S.RangedDamage = 16.0f; S.RangedInterval = 0.125f; S.RangedRange = 8500.0f;
			S.AimFOV = 60.0f; S.HipSpread = 2.4f; S.AimSpread = 0.6f; S.ViewKick = 0.38f;
			S.Rock = 14; S.Ore = 22; S.Organic = 3; S.UpgradeCostMultiplier = 1.3f; S.Accent = FLinearColor(1.0f, 0.34f, 0.18f);
			SetSilhouette(S, FVector(0.96f, 0.26f, 0.22f), FVector(20, 0, 0), FVector(0.44f, 0.12f, 0.12f), FVector(84, 0, 0),
				FVector(0.12f, 0.08f, 0.12f), FVector(30, 0, 18), FVector(0, 0, -10), FVector(108, 0, 0), FVector(0.24f, 0.18f, 0.42f));
			Result.Add(S);

			S = FWeaponSeed();
			S.Id = EJTSItemId::GrenadeLauncher; S.FireSound = TEXT("/Game/Space/Audio/S_JTSSniperFire.S_JTSSniperFire"); S.Name = TEXT("榴弹发射器"); S.Description = TEXT("中距离爆炸清群，射速慢，贴身很难使用。");
			S.bRanged = true; S.MiningWork = 3.0f; S.RangedDamage = 105.0f; S.RangedInterval = 1.333f; S.RangedRange = 6000.0f;
			S.AimFOV = 62.0f; S.HipSpread = 0.8f; S.AimSpread = 0.2f; S.ViewKick = 1.6f;
			S.Rock = 12; S.Ore = 16; S.Organic = 4; S.UpgradeCostMultiplier = 1.3f; S.Accent = FLinearColor(0.85f, 0.82f, 0.25f);
			SetSilhouette(S, FVector(0.70f, 0.22f, 0.22f), FVector(14, 0, 0), FVector(0.42f, 0.18f, 0.18f), FVector(62, 0, 0),
				FVector(0.08f, 0.06f, 0.12f), FVector(22, 0, 14), FVector(0, 0, -10), FVector(84, 0, 0), FVector(0.22f, 0.16f, 0.38f));
			Result.Add(S);

			S = FWeaponSeed();
			S.Id = EJTSItemId::ArcGun; S.FireSound = TEXT("/Game/Space/Audio/S_JTSMachineGunFire.S_JTSMachineGunFire"); S.Name = TEXT("电弧枪"); S.Description = TEXT("短距电束，在敌群之间连锁跳跃。");
			S.bRanged = true; S.MiningWork = 0.15f; S.RangedDamage = 7.5f; S.RangedInterval = 0.10f; S.RangedRange = 800.0f;
			S.AimFOV = 66.0f; S.HipSpread = 1.0f; S.AimSpread = 0.5f; S.ViewKick = 0.15f;
			S.Rock = 10; S.Ore = 14; S.Organic = 4; S.UpgradeCostMultiplier = 1.2f; S.Accent = FLinearColor(0.40f, 0.60f, 1.0f);
			SetSilhouette(S, FVector(0.62f, 0.20f, 0.18f), FVector(12, 0, 0), FVector(0.34f, 0.12f, 0.12f), FVector(52, 0, 0),
				FVector(0.06f, 0.10f, 0.10f), FVector(30, 0, 10), FVector(0, 0, -8), FVector(72, 0, 0), FVector(0.18f, 0.14f, 0.32f));
			Result.Add(S);

			return Result;
		}();
		return Seeds;
	}

	// Magazine, heat, pellet, pierce, chain and projectile baselines. Ammo stays infinite; only the magazine gates fire.
	FJTSRangedMechanics SeedMechanics(EJTSItemId ItemId)
	{
		FJTSRangedMechanics M;
		switch (ItemId)
		{
		case EJTSItemId::RailPistol:
			M.MagazineSize = 12; M.ReloadSeconds = 1.4f;
			break;
		case EJTSItemId::AssaultRifle:
			M.MagazineSize = 30; M.ReloadSeconds = 2.0f;
			break;
		case EJTSItemId::Shotgun:
			M.MagazineSize = 6; M.ReloadSeconds = 2.4f; M.Pellets = 8;
			M.FalloffStartCm = 600.0f; M.FalloffEndCm = 2000.0f; M.MinFalloffFraction = 0.3f;
			break;
		case EJTSItemId::RailSniper:
			M.MagazineSize = 5; M.ReloadSeconds = 2.8f;
			break;
		case EJTSItemId::HeavyMachineGun:
			M.MagazineSize = 100; M.ReloadSeconds = 4.2f;
			break;
		case EJTSItemId::GrenadeLauncher:
			M.MagazineSize = 4; M.ReloadSeconds = 2.8f;
			M.ProjectileSpeed = 2800.0f; M.ProjectileGravityScale = 1.0f; M.BlastRadiusCm = 350.0f; M.FragmentDamageShare = 0.4f;
			break;
		case EJTSItemId::ArcGun:
			M.EnergyCapacity = 30.0f; M.EnergyPerShot = 1.0f; M.RechargePerSecond = 6.0f; M.ResumeFraction = 0.3f;
			M.ChainTargets = 2; M.ChainRangeCm = 450.0f; M.ChainFalloff = 0.3f;
			break;
		default:
			break;
		}
		return M;
	}

	void ApplyNormalWeaponSeed(UJTSItemDefinition* Definition, EJTSItemId ItemId)
	{
		const FWeaponSeed* Seed = nullptr;
		for (const FWeaponSeed& Candidate : GetWeaponSeeds())
		{
			if (Candidate.Id == ItemId) { Seed = &Candidate; break; }
		}
		if (Seed == nullptr) return;

		Definition->DisplayName = FText::FromString(Seed->Name);
		Definition->Description = FText::FromString(Seed->Description);
		Definition->PrimaryCategory = EJTSItemCategory::Weapons;
		Definition->ShopCategories = { EJTSShopCategory::Weapons };
		Definition->AffinityTags = { FName(TEXT("Weapon")), FName(Seed->bRanged ? TEXT("Ranged") : TEXT("Melee")) };
		if (!Seed->bRanged && Seed->MiningWork >= 5.0f)
		{
			Definition->PrimaryCategory = EJTSItemCategory::Mining;
			Definition->ShopCategories = { EJTSShopCategory::Weapons, EJTSShopCategory::Mining };
			Definition->AffinityTags.Add(FName(TEXT("Mining")));
		}
		Definition->CapabilityMask = CapabilityMask({ EJTSItemCapability::Holdable, EJTSItemCapability::Mining,
			Seed->bRanged ? EJTSItemCapability::RangedWeapon : EJTSItemCapability::MeleeOverride, EJTSItemCapability::ShopPurchasable });
		Definition->MeleeAttackClass = Seed->MeleeClass;
		Definition->CombatDamage = Seed->CombatDamage;
		Definition->MeleeAttackInterval = Seed->MeleeInterval;
		Definition->MiningWork = Seed->MiningWork;
		Definition->RangedDamage = Seed->RangedDamage;
		Definition->RangedFireInterval = Seed->RangedInterval;
		Definition->RangedRange = Seed->RangedRange;
		Definition->RangedAimFOV = Seed->AimFOV;
		Definition->RangedHipSpreadDegrees = Seed->HipSpread;
		Definition->RangedAimSpreadDegrees = Seed->AimSpread;
		Definition->RangedViewKickDegrees = Seed->ViewKick;
		if (Seed->bRanged)
		{
			Definition->RangedGlowMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Space/Materials/M_JTSShotGlow.M_JTSShotGlow")));
			Definition->RangedFireSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(Seed->FireSound));
			Definition->RangedImpactSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Space/Audio/S_JTSBulletImpact.S_JTSBulletImpact")));
		}
		Definition->ShopCosts.Reset();
		if (Seed->Rock > 0) Definition->ShopCosts.Add(Cost(EJTSResourceType::Rock, Seed->Rock));
		if (Seed->Ore > 0) Definition->ShopCosts.Add(Cost(EJTSResourceType::Ore, Seed->Ore));
		if (Seed->Organic > 0) Definition->ShopCosts.Add(Cost(EJTSResourceType::Organic, Seed->Organic));
		Definition->UpgradeCostMultiplier = Seed->UpgradeCostMultiplier;
		Definition->RangedMechanics = SeedMechanics(ItemId);
		Definition->AccentColor = Seed->Accent;

		FJTSHeldItemPresentation& Held = Definition->HeldPresentation;
		Held.bOverridePrototypeProfile = true;
		Held.BodyScale = Seed->BodyScale;
		Held.BodyLocation = Seed->BodyLoc;
		Held.BarrelScale = Seed->BarrelScale;
		Held.BarrelLocation = Seed->BarrelLoc;
		Held.SightScale = Seed->SightScale;
		Held.SightLocation = Seed->SightLoc;
		Held.GripTransform = FTransform(FQuat::Identity, Seed->GripLoc, FVector::OneVector);
		Held.MuzzleTransform = FTransform(FQuat::Identity, Seed->MuzzleLoc, FVector::OneVector);
		Held.GripScale = Seed->GripScale;
	}

	UJTSItemDefinition* MakeFallback(EJTSItemId ItemId)
	{
		if (const TObjectPtr<UJTSItemDefinition>* Existing = GFallbackDefinitions.Find(ItemId))
		{
			return Existing->Get();
		}

		UJTSItemDefinition* Definition = NewObject<UJTSItemDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		Definition->AddToRoot();
		Definition->ItemId = ItemId;
		Definition->DisplayName = FText::FromString(TEXT("Unknown Item"));
		Definition->Description = FText::FromString(TEXT("Prototype item definition."));
		Definition->PrimaryCategory = EJTSItemCategory::Utility;
		Definition->MaxStackSize = 1;
		Definition->CombatDamage = 1.0f;
		Definition->MeleeAttackInterval = 0.45f;
		Definition->RangedFireInterval = 0.35f;
		Definition->RangedRange = 8000.0f;
		Definition->RangedAimFOV = 60.0f;
		Definition->AccentColor = FLinearColor(0.38f, 0.70f, 0.95f, 1.0f);

		auto SetResource = [Definition](const TCHAR* Name, EJTSItemId Id, EJTSItemCategory Category, const FLinearColor& Color)
		{
			Definition->ItemId = Id;
			Definition->DisplayName = FText::FromString(Name);
			Definition->Description = FText::FromString(TEXT("Single-unit expedition material. It can be held as an improvised melee item."));
			Definition->PrimaryCategory = Category;
			Definition->ShopCategories = { EJTSShopCategory::Resources };
			Definition->AffinityTags = { TEXT("Material"), TEXT("Improvised") };
			Definition->CapabilityMask = CapabilityMask({ EJTSItemCapability::Holdable, EJTSItemCapability::StackableResource });
			Definition->MaxStackSize = 1;
			Definition->CombatDamage = 1.0f;
			Definition->AccentColor = Color;
		};

		switch (ItemId)
		{
		case EJTSItemId::Rock:
			SetResource(TEXT("Rock"), ItemId, EJTSItemCategory::Resources, FLinearColor(0.48f, 0.50f, 0.56f, 1.0f));
			break;
		case EJTSItemId::Ore:
			SetResource(TEXT("Ore"), ItemId, EJTSItemCategory::Resources, FLinearColor(0.10f, 0.72f, 0.95f, 1.0f));
			Definition->AffinityTags = { TEXT("Material"), TEXT("Conductive"), TEXT("Improvised") };
			break;
		case EJTSItemId::Fuel:
			SetResource(TEXT("Fuel"), ItemId, EJTSItemCategory::Resources, FLinearColor(1.0f, 0.44f, 0.12f, 1.0f));
			break;
		case EJTSItemId::Water:
			SetResource(TEXT("Water"), ItemId, EJTSItemCategory::Resources, FLinearColor(0.14f, 0.55f, 1.0f, 1.0f));
			break;
		case EJTSItemId::Food:
			SetResource(TEXT("Food"), ItemId, EJTSItemCategory::Resources, FLinearColor(0.34f, 0.90f, 0.38f, 1.0f));
			break;
		case EJTSItemId::MoonAntCorpse:
			SetResource(TEXT("Moon Ant Corpse"), ItemId, EJTSItemCategory::Resources, FLinearColor(0.70f, 0.30f, 0.72f, 1.0f));
			Definition->MaxStackSize = 1;
			break;
		case EJTSItemId::WaistLamp:
			Definition->DisplayName = FText::FromString(TEXT("头灯"));
			Definition->Description = FText::FromString(TEXT("戴在额头的便携头灯。选中后按 F 开关，灯光跟着视线，双手保持空闲。"));
			Definition->PrimaryCategory = EJTSItemCategory::Utility;
			Definition->ShopCategories = { EJTSShopCategory::Utility };
			Definition->AffinityTags = { TEXT("Light"), TEXT("Worn"), TEXT("Utility") };
			Definition->CapabilityMask = CapabilityMask({ EJTSItemCapability::ShopPurchasable });
			Definition->CombatDamage = 0.0f;
			Definition->MiningWork = 0.0f;
			Definition->ShopCosts = { Cost(EJTSResourceType::Rock, 2), Cost(EJTSResourceType::Ore, 2) };
			Definition->AccentColor = FLinearColor(0.92f, 0.95f, 1.0f, 1.0f);
			break;
		case EJTSItemId::StellarText:
			Definition->DisplayName = FText::FromString(TEXT("星际道具"));
			Definition->Description = FText::FromString(TEXT("星际联盟遥感获得的文字版道具。"));
			Definition->PrimaryCategory = EJTSItemCategory::Utility;
			Definition->CapabilityMask = 0;
			Definition->CombatDamage = 0.0f;
			Definition->AccentColor = FLinearColor(0.40f, 0.84f, 0.92f, 1.0f);
			break;
		case EJTSItemId::StellarWeapon:
			Definition->DisplayName = FText::FromString(TEXT("星际武器"));
			Definition->Description = FText::FromString(TEXT("由匹配的核心与配件在飞船物品栏组合。"));
			Definition->PrimaryCategory = EJTSItemCategory::Weapons;
			Definition->CapabilityMask = CapabilityMask({ EJTSItemCapability::Holdable, EJTSItemCapability::MeleeOverride });
			Definition->CombatDamage = 2.0f;
			Definition->AccentColor = FLinearColor(0.40f, 0.84f, 0.92f, 1.0f);
			break;
		default:
			ApplyNormalWeaponSeed(Definition, ItemId);
			break;
		}

		GFallbackDefinitions.Add(ItemId, Definition);
		return Definition;
	}
}

UJTSItemDefinition* UJTSItemDefinitionLibrary::GetItemDefinition(const UObject* /*WorldContextObject*/, EJTSItemId ItemId)
{
	if (!IsGameplayItemAvailable(ItemId))
	{
		return nullptr;
	}
	if (const TStrongObjectPtr<UJTSItemDefinition>* Existing = GLoadedDefinitions.Find(ItemId))
	{
		if (IsValid(Existing->Get())) return Existing->Get();
		GLoadedDefinitions.Remove(ItemId);
	}

	const FString AssetPath = AssetPathFor(ItemId);
	// Definitions are optional overrides. Do not ask the loader to load a missing package
	// before using the native default; still load existing assets normally so real errors are reported.
	if (!AssetPath.IsEmpty() && (FindObject<UObject>(nullptr, *AssetPath) != nullptr
		|| FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(AssetPath))))
	{
		if (UJTSItemDefinition* const Loaded = LoadObject<UJTSItemDefinition>(nullptr, *AssetPath);
			IsValid(Loaded) && Loaded->ItemId == ItemId)
		{
			GLoadedDefinitions.Add(ItemId, TStrongObjectPtr<UJTSItemDefinition>(Loaded));
			return Loaded;
		}
	}

	UJTSItemDefinition* const Fallback = MakeFallback(ItemId);
	GLoadedDefinitions.Add(ItemId, TStrongObjectPtr<UJTSItemDefinition>(Fallback));
	return Fallback;
}

FText UJTSItemDefinitionLibrary::GetItemDisplayName(EJTSItemId ItemId)
{
	if (const UJTSItemDefinition* const Definition = GetItemDefinition(nullptr, ItemId))
	{
		return Definition->DisplayName;
	}
	return FText::FromString(ItemId == EJTSItemId::Backpack ? TEXT("Retired Item") : TEXT("Empty"));
}

bool UJTSItemDefinitionLibrary::TryGetResourceType(EJTSItemId ItemId, EJTSResourceType& OutResourceType)
{
	switch (ItemId)
	{
	case EJTSItemId::Fuel: OutResourceType = EJTSResourceType::Fuel; return true;
	case EJTSItemId::Water: OutResourceType = EJTSResourceType::Water; return true;
	case EJTSItemId::Food: OutResourceType = EJTSResourceType::Food; return true;
	case EJTSItemId::Rock: OutResourceType = EJTSResourceType::Rock; return true;
	case EJTSItemId::Ore: OutResourceType = EJTSResourceType::Ore; return true;
	case EJTSItemId::MoonAntCorpse: OutResourceType = EJTSResourceType::MoonAntCorpse; return true;
	default: return false;
	}
}

EJTSItemId UJTSItemDefinitionLibrary::GetItemIdForResource(EJTSResourceType ResourceType)
{
	switch (ResourceType)
	{
	case EJTSResourceType::Fuel: return EJTSItemId::Fuel;
	case EJTSResourceType::Water: return EJTSItemId::Water;
	case EJTSResourceType::Food: return EJTSItemId::Food;
	case EJTSResourceType::Rock: return EJTSItemId::Rock;
	case EJTSResourceType::Ore: return EJTSItemId::Ore;
	case EJTSResourceType::MoonAntCorpse: return EJTSItemId::MoonAntCorpse;
	default: return EJTSItemId::None;
	}
}

FJTSItemInstance UJTSItemDefinitionLibrary::MakeInstance(EJTSItemId ItemId, int32 Count)
{
	FJTSItemInstance Result;
	if (!IsGameplayItemAvailable(ItemId) || Count <= 0)
	{
		return Result;
	}

	Result.ItemId = ItemId;
	Result.StackCount = Count;
	if (const UJTSItemDefinition* const Definition = GetItemDefinition(nullptr, ItemId))
	{
		Result.Durability = Definition->DefaultDurability;
	}
	Result.InstanceId = FGuid::NewGuid();
	return Result;
}

bool UJTSItemDefinitionLibrary::IsGameplayItemAvailable(const EJTSItemId ItemId)
{
	switch (ItemId)
	{
	case EJTSItemId::None:
	case EJTSItemId::Backpack:
	// Retired test weapons: old saves may still hold them, but they are never obtainable or usable again.
	case EJTSItemId::Pickaxe:
	case EJTSItemId::Knife:
	case EJTSItemId::Pistol:
	case EJTSItemId::MachineGun:
	case EJTSItemId::Axe:
	case EJTSItemId::Sniper:
	case EJTSItemId::IceAxe:
		return false;
	default:
		return true;
	}
}

const TArray<EJTSItemId>& UJTSItemDefinitionLibrary::GetDefaultShopCatalog()
{
	static const TArray<EJTSItemId> Catalog = {
		EJTSItemId::ShortBlade,
		EJTSItemId::ShockPole,
		EJTSItemId::PowerHammer,
		EJTSItemId::RailPistol,
		EJTSItemId::AssaultRifle,
		EJTSItemId::Shotgun,
		EJTSItemId::RailSniper,
		EJTSItemId::HeavyMachineGun,
		EJTSItemId::GrenadeLauncher,
		EJTSItemId::ArcGun,
		EJTSItemId::WaistLamp
	};
	return Catalog;
}
