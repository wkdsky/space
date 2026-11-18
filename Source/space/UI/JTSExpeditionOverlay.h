#pragma once
#include "CoreMinimal.h"
class UUserWidget;
class FSlateWindowElementList;
struct FGeometry;
/** Local presentation of replicated expedition state. */
struct FJTSExpeditionOverlay
{
    static int32 Paint(const UUserWidget* Widget,const FGeometry& G,FSlateWindowElementList& E,int32 Layer);
};
