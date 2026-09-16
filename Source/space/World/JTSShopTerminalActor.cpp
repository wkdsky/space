// Copyright Epic Games, Inc. All Rights Reserved.

#include "space/World/JTSShopTerminalActor.h"

#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "space/Components/JTSInventoryComponent.h"
#include "space/Components/JTSPlayerEquipmentComponent.h"
#include "space/Items/JTSItemDefinition.h"
#include "space/Items/JTSItemDefinitionLibrary.h"
#include "space/Items/JTSWorldPickupActor.h"
#include "space/Player/JTSCharacter.h"
#include "space/Player/JTSPlayerController.h"
#include "space/Ships/JTSSpacecraftActor.h"
#include "UObject/ConstructorHelpers.h"

AJTSShopTerminalActor::AJTSShopTerminalActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	TerminalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TerminalMesh"));
	SetRootComponent(TerminalMesh);
	TerminalMesh->SetRelativeScale3D(FVector(0.7f, 0.5f, 1.35f));
	TerminalMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TerminalMesh->SetCollisionObjectType(ECC_WorldDynamic);
	TerminalMesh->SetCollisionResponseToAllChannels(ECR_Overlap);
	TerminalMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	TerminalMesh->SetGenerateOverlapEvents(true);
	TerminalMesh->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> TerminalMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (TerminalMeshAsset.Succeeded())
	{
		TerminalMesh->SetStaticMesh(TerminalMeshAsset.Object);
	}
}

void AJTSShopTerminalActor::BeginPlay()
{
	Super::BeginPlay();
	RefreshVisuals();
}

void AJTSShopTerminalActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AJTSShopTerminalActor, SharedSpacecraft);
}

void AJTSShopTerminalActor::InitializeTerminal(AJTSSpacecraftActor* InSharedSpacecraft)
{
	if (HasAuthority())
	{
		SharedSpacecraft = InSharedSpacecraft;
	}
}

AJTSSpacecraftActor* AJTSShopTerminalActor::GetSharedSpacecraft() const
{
	return SharedSpacecraft.Get();
}

int32 AJTSShopTerminalActor::GetSharedResourceAmount(EJTSResourceType ResourceType) const
{
	return IsValid(SharedSpacecraft) ? SharedSpacecraft->GetResourceAmount(ResourceType) : 0;
}

bool AJTSShopTerminalActor::IsPlayerInTerminalRange(const APawn* Player) const
{
	return IsValid(Player)
		&& FVector::DistSquared(Player->GetActorLocation(), GetActorLocation()) <= FMath::Square(FMath::Max(100.0f, InteractionDistance));
}

bool AJTSShopTerminalActor::CanInteract_Implementation(APawn* InteractingPawn) const
{
	return !IsPendingKillPending() && IsPlayerInTerminalRange(InteractingPawn) && IsValid(SharedSpacecraft);
}

FText AJTSShopTerminalActor::GetInteractionPrompt_Implementation(APawn* InteractingPawn) const
{
	return CanInteract_Implementation(InteractingPawn)
		? FText::FromString(TEXT("[E] OPEN EXPEDITION SUPPLY"))
		: FText::GetEmpty();
}

void AJTSShopTerminalActor::Interact_Implementation(APawn* InteractingPawn)
{
	if (!HasAuthority() || !CanInteract_Implementation(InteractingPawn))
	{
		return;
	}
	if (AJTSPlayerController* const Controller = Cast<AJTSPlayerController>(InteractingPawn->GetController()))
	{
		Controller->ClientOpenSpaceShop(this);
	}
}

bool AJTSShopTerminalActor::TryDepositPlayerMaterials(AJTSCharacter* Player)
{
	if (!HasAuthority() || !IsValid(Player) || !IsPlayerInTerminalRange(Player) || !IsValid(SharedSpacecraft))
	{
		return false;
	}
	UJTSInventoryComponent* const Inventory = Player->GetInventoryComponent();
	if (!IsValid(Inventory))
	{
		return false;
	}

	TMap<EJTSResourceType, int32> Materials;
	const int32 RockCount = Inventory->GetItemCount(EJTSItemId::Rock);
	const int32 OreCount = Inventory->GetItemCount(EJTSItemId::Ore);
	if (RockCount > 0) { Materials.Add(EJTSResourceType::Rock, RockCount); }
	if (OreCount > 0) { Materials.Add(EJTSResourceType::Ore, OreCount); }
	if (Materials.IsEmpty())
	{
		return false;
	}

	const bool bRemovedRock = RockCount <= 0 || Inventory->TryRemoveItem(EJTSItemId::Rock, RockCount);
	const bool bRemovedOre = bRemovedRock && (OreCount <= 0 || Inventory->TryRemoveItem(EJTSItemId::Ore, OreCount));
	if (!bRemovedRock || !bRemovedOre)
	{
		if (bRemovedRock && RockCount > 0) { Inventory->TryAddItemById(EJTSItemId::Rock, RockCount); }
		if (bRemovedOre && OreCount > 0) { Inventory->TryAddItemById(EJTSItemId::Ore, OreCount); }
		return false;
	}
	if (!SharedSpacecraft->DepositResourceAmounts(Materials))
	{
		if (RockCount > 0) { Inventory->TryAddItemById(EJTSItemId::Rock, RockCount); }
		if (OreCount > 0) { Inventory->TryAddItemById(EJTSItemId::Ore, OreCount); }
		return false;
	}
	return true;
}

bool AJTSShopTerminalActor::BuildCosts(EJTSItemId ItemId, TMap<EJTSResourceType, int32>& OutCosts) const
{
	OutCosts.Reset();
	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, ItemId);
	if (!IsValid(Definition) || !Definition->IsShopPurchasable())
	{
		return false;
	}
	for (const FJTSItemCost& Cost : Definition->ShopCosts)
	{
		if (Cost.Amount > 0)
		{
			OutCosts.FindOrAdd(Cost.ResourceType) += Cost.Amount;
		}
	}
	return !OutCosts.IsEmpty();
}

bool AJTSShopTerminalActor::DeliverPurchase(AJTSCharacter* Player, const FJTSItemInstance& Item, bool& bOutDropped)
{
	bOutDropped = false;
	if (!IsValid(Player) || Item.IsEmpty())
	{
		return false;
	}
	const UJTSItemDefinition* const Definition = UJTSItemDefinitionLibrary::GetItemDefinition(this, Item.ItemId);
	if (!IsValid(Definition))
	{
		return false;
	}

	bool bDelivered = false;
	if (Definition->IsWearable())
	{
		if (UJTSPlayerEquipmentComponent* const Wearables = Player->GetEquipmentComponent())
		{
			bDelivered = Wearables->TryEquipItem(Item);
		}
	}
	else if (UJTSInventoryComponent* const Inventory = Player->GetInventoryComponent())
	{
		if (Inventory->CanAddItem(Item.ItemId, Item.StackCount))
		{
			int32 Remaining = Item.StackCount;
			bDelivered = Inventory->TryAddItem(Item, Remaining) && Remaining == 0;
		}
	}

	if (bDelivered)
	{
		return true;
	}

	AJTSWorldPickupActor* const Pickup = AJTSWorldPickupActor::SpawnGameplayDrop(
		GetWorld(), Item, GetActorLocation(), Player, this, Player->GetActorForwardVector());
	bOutDropped = IsValid(Pickup);
	return bOutDropped;
}

EJTSShopPurchaseResult AJTSShopTerminalActor::TryPurchase(AJTSCharacter* Player, EJTSItemId ItemId)
{
	if (!HasAuthority() || !IsValid(Player) || !IsPlayerInTerminalRange(Player) || !IsValid(SharedSpacecraft))
	{
		return EJTSShopPurchaseResult::DeliveryFailed;
	}
	TMap<EJTSResourceType, int32> Costs;
	if (!BuildCosts(ItemId, Costs))
	{
		return EJTSShopPurchaseResult::InvalidItem;
	}
	if (!SharedSpacecraft->TryConsumeResourceAmounts(Costs))
	{
		return EJTSShopPurchaseResult::InsufficientResources;
	}

	bool bDropped = false;
	if (!DeliverPurchase(Player, UJTSItemDefinitionLibrary::MakeInstance(ItemId), bDropped))
	{
		// Purchase is atomic from the player's perspective: failed delivery refunds the shared wallet.
		SharedSpacecraft->DepositResourceAmounts(Costs);
		return EJTSShopPurchaseResult::DeliveryFailed;
	}
	return bDropped ? EJTSShopPurchaseResult::SucceededDropped : EJTSShopPurchaseResult::Succeeded;
}

void AJTSShopTerminalActor::RefreshVisuals()
{
	if (!IsValid(TerminalMesh))
	{
		return;
	}
	TerminalMaterial = TerminalMesh->CreateAndSetMaterialInstanceDynamic(0);
	if (IsValid(TerminalMaterial))
	{
		TerminalMaterial->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.06f, 0.34f, 0.55f, 1.0f));
		TerminalMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.06f, 0.34f, 0.55f, 1.0f));
	}
}
