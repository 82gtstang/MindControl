#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "CryptGameMode.generated.h"
class UMaterialInterface;
class UStaticMesh;

UCLASS()
class MINDCONTROL_API ACryptPlayerController : public APlayerController
{
 GENERATED_BODY()
protected:
 virtual TSharedPtr<class SVirtualJoystick> CreateVirtualJoystick() override;
};

UCLASS()
class MINDCONTROL_API ACryptGameMode : public AGameModeBase
{
 GENERATED_BODY()
public:
 ACryptGameMode();
protected:
 virtual void BeginPlay() override;
 void PlaceBlock(const FVector& Location,const FVector& Scale,const FRotator& Rotation=FRotator::ZeroRotator,
  UMaterialInterface* Material=nullptr,UStaticMesh* Mesh=nullptr);
};
