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
 bool bRoadRaider=false;
 int32 SaveId=INDEX_NONE;
 float GetRemainingHealth() const { return Health; }
 void RestoreState(float SavedHealth,const FVector& Location);
protected:
 UPROPERTY(VisibleAnywhere) class USphereComponent* Collision;
 UPROPERTY(VisibleAnywhere) class UStaticMeshComponent* Body;
 float Health=3.f;
};
