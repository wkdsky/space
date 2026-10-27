#include "space/Components/JTSAvatarAppearanceComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

void UJTSAvatarAppearanceComponent::Apply(const FJTSAvatarAppearance& A)
{
 if (!A.IsValid() || !Catalog || Catalog->Suits.Num()!=5 || Catalog->Colors.Num()!=5) return;
 USkeletalMeshComponent* Mesh=GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
 if (!Mesh || (bApplied && AppliedMesh==Mesh && Last==A && Mesh->GetSkeletalMeshAsset()==Catalog->Suits[A.Suit])) return;
 if (Mesh->GetSkeletalMeshAsset()!=Catalog->Suits[A.Suit]) Mesh->SetSkeletalMesh(Catalog->Suits[A.Suit]);
 const TArray<FName> Slots=Mesh->GetMaterialSlotNames();
 for (int32 I=0; I<Slots.Num(); ++I)
 {
  UMaterialInstanceDynamic* MID=Mesh->CreateAndSetMaterialInstanceDynamic(I);
  if (!MID) continue;
  const FName Slot=Slots[I];
  if (Slot==TEXT("FE_Helmet")) MID->SetVectorParameterValue(TEXT("Tint"),Catalog->Colors[A.Helmet]);
  if (Slot==TEXT("FE_Upper")) MID->SetVectorParameterValue(TEXT("Tint"),Catalog->Colors[A.Upper]);
  if (Slot==TEXT("FE_Lower")) MID->SetVectorParameterValue(TEXT("Tint"),Catalog->Colors[A.Lower]);
  if (Slot==TEXT("FE_Eye") || Slot==TEXT("FE_Mouth"))
  {
   const int32 Cell=Slot==TEXT("FE_Eye") ? A.Eyes : 5+A.Mouth;
   MID->SetVectorParameterValue(TEXT("UVOffset"),FLinearColor((Cell%4)*.25f+32.f/2048.f,(Cell/4)*.25f+32.f/2048.f,0,0));
   MID->SetVectorParameterValue(TEXT("UVScale"),FLinearColor(448.f/2048.f,448.f/2048.f,0,0));
  }
 }
 Last=A; AppliedMesh=Mesh; bApplied=true;
}
