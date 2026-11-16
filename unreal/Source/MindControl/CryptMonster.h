#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CryptMonster.generated.h"
UCLASS()
class MINDCONTROL_API ACryptMonster : public AActor
{
 GENERATED_BODY()
public:
 ACryptMonster();
 virtual void Tick(float DeltaSeconds) override;
 void TakeHit(float Damage);
protected:
 UPROPERTY(VisibleAnywhere) class USphereComponent* Collision;
 UPROPERTY(VisibleAnywhere) class UStaticMeshComponent* Body;
 float Health=3.f;
};
