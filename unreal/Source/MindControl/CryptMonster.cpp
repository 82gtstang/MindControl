#include "CryptMonster.h"
#include "CryptHero.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ACryptMonster::ACryptMonster()
{
 PrimaryActorTick.bCanEverTick=true;
 PrimaryActorTick.TickInterval=.075f;
 Collision=CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
 RootComponent=Collision;
 Collision->InitSphereRadius(44.f);
 Collision->SetCollisionProfileName(TEXT("Pawn"));
 Collision->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
 Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MonsterBody"));
 Body->SetupAttachment(RootComponent);
 Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);

}

void ACryptMonster::ConfigureCreature(int32 Index,int32 InDungeon,bool bInBoss)
{
 Creature=FMath::Clamp(Index,0,int32(UE_ARRAY_COUNT(CryptCombat::Creatures))-1);
 Dungeon=InDungeon;
 bBoss=bInBoss;
 Profile=CryptCombat::Creatures[Creature];
 if(bBoss)
 {
  Profile.Health*=2.4f;
  Profile.Weapon*=1.20f;
  Profile.Armor+=3.f;
 }
 Health=Profile.Health;
 Stamina=Profile.Stamina;
 Mana=Profile.Mana;
 Home=GetActorLocation();
 const FString Model=FString::Printf(TEXT("/Game/Characters/Town/DungeonAssets/Enemy%02d.Enemy%02d"),Creature,Creature);
 auto* Mesh=LoadObject<UStaticMesh>(nullptr,*Model);
 if(!ensureMsgf(Mesh,TEXT("Missing Blender creature %s"),*Model)) return;
 Body->SetStaticMesh(Mesh);
 const auto Bounds=Mesh->GetBounds();
 const double Height=bBoss?365.:Creature==2||Creature==4?100.:Creature==5?205.:195.;
 const double Scale=Height/FMath::Max(1.,Bounds.BoxExtent.Z*2.);
 Body->SetRelativeScale3D(FVector(Scale));
 Body->SetRelativeLocation(FVector(0,0,-95-(Bounds.Origin.Z-Bounds.BoxExtent.Z)*Scale));
 Body->SetRelativeRotation(FRotator(0,-90,0));
 if(bBoss) Collision->SetSphereRadius(72.);

}

void ACryptMonster::Tick(float Dt)
{
 Super::Tick(Dt);
 if(PoisonTicks || BurnTicks)
 {
  EffectTimer-=Dt;
  if(EffectTimer<=0.f)
  {
   EffectTimer+=1.f;
   if(PoisonTicks>0)
   {
    --PoisonTicks;
    TakeHit(5.f,0.f,ECryptDamage::Poison);
   }
   if(BurnTicks>0 && Health>0.f)
   {
    --BurnTicks;
    if(auto* Hero=Cast<ACryptHero>(UGameplayStatics::GetPlayerPawn(this,0)))
     Hero->ShowSpell(GetActorLocation(),GetActorLocation()+FVector(0,0,65),ECryptDamage::Fire,FLinearColor(1.f,.28f,.035f));
    TakeHit(7.f,0.f,ECryptDamage::Fire);
   }
   if(Health<=0.f) return;
  }
 }
 auto* Player=Cast<ACryptHero>(UGameplayStatics::GetPlayerPawn(this,0));
 if(!Player || Health<=0.f) return;
 const auto& CombatProfile=Stats();
 FVector Offset=Player->GetActorLocation()-GetActorLocation();
 if(FMath::Abs(Offset.Z)>165.f || Offset.SizeSquared2D()>FMath::Square(1450.f)) return;
 Offset.Z=0;
 const float Distance=Offset.Size();
 AttackTimer=FMath::Max(0.f,AttackTimer-Dt);
 Stamina=FMath::Min(CombatProfile.Stamina,Stamina+Dt*10);
 Mana=FMath::Min(CombatProfile.Mana,Mana+Dt*4);
 const bool bRanged=CombatProfile.Element!=ECryptDamage::Physical && Mana>=14.f;
 const float Reach=bRanged?550.f:155.f;
 SetActorRotation(Offset.Rotation());
 if(Distance>Reach*.87f && (Home.IsNearlyZero() || FVector::DistSquared2D(Home,GetActorLocation())<FMath::Square(1250.f)))
  SetActorLocation(GetActorLocation()+Offset.GetSafeNormal()*FMath::Min(CombatProfile.Speed*Dt,Distance-Reach*.87f),true);
 if(Distance>Reach || AttackTimer>0.f || Stamina<10.f) return;
 FCollisionQueryParams Query;
 Query.AddIgnoredActor(this);
 Query.AddIgnoredActor(Player);
 const FVector From=GetActorLocation()+FVector(0,0,28);
 if(GetWorld()->LineTraceTestByChannel(From,Player->GetActorLocation(),ECC_Visibility,Query)) return;
 AttackTimer=FMath::Clamp(2.f-CombatProfile.Dexterity*.028f,1.15f,1.95f);
 Stamina-=10.f;
 if(bRanged)
 {
  Mana-=14.f;
  Player->ShowSpell(From,Player->GetActorLocation(),CombatProfile.Element,CombatProfile.Color);
 }
 Player->NoticeEnemy(this);
 Player->ReceiveCombatHit(CombatProfile.Weapon,CombatProfile.Dexterity,bRanged?CombatProfile.Element:ECryptDamage::Physical);
}

bool ACryptMonster::TakeHit(float Rating,float Dexterity,ECryptDamage Element)
{
 if(Health<=0.f) return false;
 const auto& CombatProfile=Stats();
 if(Element==ECryptDamage::Physical && FMath::FRand()>CryptCombat::HitChance(Dexterity,CombatProfile.Dexterity)) return false;
 const float Multiplier=Element==ECryptDamage::Fire?CombatProfile.Fire:Element==ECryptDamage::Storm?CombatProfile.Storm:1.f;
 const float Before=Health;
 Health=FMath::Max(0.f,Health-CryptCombat::Damage(Rating,CombatProfile.Armor,Element)*Multiplier);
 if(auto* Hero=Cast<ACryptHero>(UGameplayStatics::GetPlayerPawn(this,0)))
  Hero->ReportEnemyHit(this,Before-Health);
 if(Health>0.f) return true;
 if(bRoadRaider)
  if(auto* Hero=Cast<ACryptHero>(UGameplayStatics::GetPlayerPawn(this,0))) Hero->DefeatedRoadRaider();
 RestoreState(0.f,GetActorLocation());
 if(Dungeon!=INDEX_NONE)
  if(auto* Hero=Cast<ACryptHero>(UGameplayStatics::GetPlayerPawn(this,0))) Hero->CheckDungeonClear(Dungeon);
 return true;
}

void ACryptMonster::ApplyArrowEffect(bool bFire)
{
 if(Health<=0.f) return;
 if(!PoisonTicks && !BurnTicks) EffectTimer=1.f;
 if(bFire) BurnTicks=3;
 else PoisonTicks=4;
}

void ACryptMonster::RestoreState(float SavedHealth,const FVector& Location)
{
 PoisonTicks=0;
 BurnTicks=0;
 Health=FMath::Clamp(SavedHealth,0.f,Stats().Health);
 SetActorLocation(Location,false,nullptr,ETeleportType::TeleportPhysics);
 const bool bAlive=Health>0.f;
 SetActorHiddenInGame(!bAlive);
 SetActorEnableCollision(bAlive);
 SetActorTickEnabled(bAlive);
}
