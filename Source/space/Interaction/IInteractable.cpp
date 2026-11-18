#include "space/Interaction/IInteractable.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "space/World/JTSSurfacePlacementBounds.h"

FVector IInteractable::GetInteractionPromptAnchor_Implementation() const
{
    const auto* Actor = Cast<AActor>(this);
    if (!Actor) return FVector::ZeroVector;
    const FVector Up = Actor->GetActorUpVector();
    FVector Anchor = Actor->GetActorLocation() + Up * 24.f;
    TInlineComponentArray<UStaticMeshComponent*> Meshes(Actor);
    for (const auto* Mesh : Meshes)
    {
        FVector Top;
        if (Mesh->IsVisible() && Mesh->GetStaticMesh()
            && JTSSurfacePlacementBounds::GetVisualTopCenter(Mesh, Up, 18.f, Top)
            && FVector::DotProduct(Top - Anchor, Up) > 0)
            Anchor = Top;
    }
    return Anchor;
}
