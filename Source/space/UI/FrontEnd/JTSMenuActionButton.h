#pragma once
#include "CoreMinimal.h"
#include "Components/Button.h"
#include "JTSMenuActionButton.generated.h"
DECLARE_MULTICAST_DELEGATE_TwoParams(FJTSMenuAction,FName,int32);

/** Editor-authored action identifier; avoids one callback per visual option. */
UCLASS()
class SPACE_API UJTSMenuActionButton : public UButton
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Menu") FName Action;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Menu") int32 Index = 0;
 UPROPERTY(EditAnywhere, Category="Menu") TObjectPtr<class USoundBase> FocusSound;
 UPROPERTY(EditAnywhere, Category="Menu") TObjectPtr<class USoundBase> ClickSound;
 FJTSMenuAction OnAction;
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
private:
 UFUNCTION() void Invoke();
 UFUNCTION() void Hover();
};
