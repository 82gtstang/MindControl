#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CryptMobileHUD.generated.h"
UCLASS()
class MINDCONTROL_API ACryptMobileHUD : public AHUD
{
 GENERATED_BODY()
public:
 virtual void DrawHUD() override;
};
