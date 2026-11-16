#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CryptGameMode.generated.h"
UCLASS()
class MINDCONTROL_API ACryptGameMode : public AGameModeBase
{
 GENERATED_BODY()
public:
 ACryptGameMode();
protected:
 virtual void BeginPlay() override;
 void PlaceBlock(const FVector& Location,const FVector& Scale,const FRotator& Rotation=FRotator::ZeroRotator);
};
