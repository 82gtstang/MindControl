#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CryptCombat.h"
#include "CryptMonster.generated.h"
UCLASS()
class MINDCONTROL_API ACryptMonster : public AActor
{
 GENERATED_BODY()
public:
 ACryptMonster();
 virtual void Tick(float DeltaSeconds) override;
 void ApplyArrowEffect(bool bFire);
 bool TakeHit(float Rating,float Dexterity,ECryptDamage Element);
 void ConfigureCreature(int32 Index,int32 InDungeon=INDEX_NONE,bool bInBoss=false);
 const CryptCombat::FCreature& Stats() const { return Profile; }
 bool IsBoss() const { return bBoss; }
 int32 GetDungeon() const { return Dungeon; }
 bool bRoadRaider=false;
 int32 SaveId=INDEX_NONE;
 float GetRemainingHealth() const { return Health; }
 void RestoreState(float SavedHealth,const FVector& Location);
protected:
 UPROPERTY(VisibleAnywhere) class USphereComponent* Collision;
 UPROPERTY(VisibleAnywhere) class UStaticMeshComponent* Body;
 int32 Creature=0;
 bool bBoss=false;
 CryptCombat::FCreature Profile=CryptCombat::Creatures[0];
 int32 Dungeon=INDEX_NONE;
 float Health=30.f, Stamina=45.f, Mana=0.f, AttackTimer=0.f;
 float EffectTimer=0.f;
 int32 PoisonTicks=0, BurnTicks=0;
 FVector Home=FVector::ZeroVector;
};
