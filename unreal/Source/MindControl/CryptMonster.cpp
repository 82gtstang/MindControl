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
 Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MonsterBody"));
 Body->SetupAttachment(RootComponent);
 Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
 Body->SetStaticMesh(Sphere.Object);
 Body->SetRelativeScale3D(FVector(1.2,1.2,1.8));
}

void ACryptMonster::ConfigureCreature(int32 Index,int32 InDungeon)
{
 Creature=FMath::Clamp(Index,0,int32(UE_ARRAY_COUNT(CryptCombat::Creatures))-1);
 Dungeon=InDungeon;
 const auto& Profile=Stats();
 Health=Profile.Health;
 Stamina=Profile.Stamina;
 Mana=Profile.Mana;
 Home=GetActorLocation();
 auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
 UMaterialInstanceDynamic* Hide=Base?UMaterialInstanceDynamic::Create(Base,this):nullptr;
 if(Hide)
 {
  Hide->SetVectorParameterValue(TEXT("Color"),Profile.Color);
  Body->SetMaterial(0,Hide);
 }
 const auto Part=[&](FVector Location,FVector Scale,FRotator Rotation=FRotator::ZeroRotator)
 {
  auto* Mesh=NewObject<UStaticMeshComponent>(this);
  AddInstanceComponent(Mesh);
  Mesh->SetupAttachment(RootComponent);
  Mesh->SetStaticMesh(Body->GetStaticMesh());
  Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Mesh->SetRelativeTransform(FTransform(Rotation,Location,Scale));
  if(Hide) Mesh->SetMaterial(0,Hide);
  Mesh->RegisterComponent();
 };
 if(Creature==2 || Creature==4)
 {
  Body->SetRelativeScale3D(FVector(1.35,1.05,.55));
  Body->SetRelativeLocation(FVector(0,0,-38));
  Part(FVector(68,0,-39),FVector(.60,.72,.50));
  for(int32 Leg=0;Leg<(Creature==2?8:6);++Leg)
  {
   const int32 Pair=Leg/2;
   const float Side=Leg%2?1.f:-1.f;
   Part(FVector(40-31*Pair,Side*65,-49),FVector(.12,.79,.15),FRotator(0,Side*(20+Pair*12),0));
  }
 }
 else if(Creature==5)
 {
  Body->SetRelativeScale3D(FVector(.63,.63,.77));
  Body->SetRelativeLocation(FVector(0,0,17));
  Part(FVector(-11,0,-31),FVector(.32,.32,.59));
 }
 else if(Creature==8)
 {
  Body->SetRelativeScale3D(FVector(1.62,.77,.78));
  Part(FVector(75,0,19),FVector(.91,.48,.5));
  Part(FVector(-90,0,-16),FVector(1.23,.22,.22),FRotator(-10,0,0));
  for(float Side : {-1.f,1.f})
  {
   Part(FVector(-15,Side*74,41),FVector(.88,1.35,.15),FRotator(0,Side*30,-Side*25));
   Part(FVector(30,Side*37,-42),FVector(.31,.32,.51));
   Part(FVector(-37,Side*37,-42),FVector(.31,.32,.51));
  }
 }
 else
 {
  const float Size=Creature==11?1.34f:Creature==3?.75f:1.f;
  Body->SetRelativeScale3D(FVector(.68,.82,.98)*Size);
  Part(FVector(10,0,61)*Size,FVector(.47,.46,.49)*Size);
  for(float Side : {-1.f,1.f})
  {
   Part(FVector(0,Side*48,4)*Size,FVector(.26,.29,.77)*Size,FRotator(0,0,Side*12));
   Part(FVector(0,Side*19,-48)*Size,FVector(.33,.32,.63)*Size);
  }
 }
}

void ACryptMonster::Tick(float Dt)
{
 Super::Tick(Dt);
 auto* Player=Cast<ACryptHero>(UGameplayStatics::GetPlayerPawn(this,0));
 if(!Player || Health<=0.f) return;
 const auto& Profile=Stats();
 FVector Offset=Player->GetActorLocation()-GetActorLocation();
 if(FMath::Abs(Offset.Z)>165.f || Offset.SizeSquared2D()>FMath::Square(1450.f)) return;
 Offset.Z=0;
 const float Distance=Offset.Size();
 AttackTimer=FMath::Max(0.f,AttackTimer-Dt);
 Stamina=FMath::Min(Profile.Stamina,Stamina+Dt*10);
 Mana=FMath::Min(Profile.Mana,Mana+Dt*4);
 const bool bRanged=Profile.Element!=ECryptDamage::Physical && Mana>=14.f;
 const float Reach=bRanged?550.f:155.f;
 SetActorRotation(Offset.Rotation());
 if(Distance>Reach*.87f && (Home.IsNearlyZero() || FVector::DistSquared2D(Home,GetActorLocation())<FMath::Square(1250.f)))
  SetActorLocation(GetActorLocation()+Offset.GetSafeNormal()*FMath::Min(Profile.Speed*Dt,Distance-Reach*.87f),true);
 if(Distance>Reach || AttackTimer>0.f || Stamina<10.f) return;
 FCollisionQueryParams Query;
 Query.AddIgnoredActor(this);
 Query.AddIgnoredActor(Player);
 const FVector From=GetActorLocation()+FVector(0,0,28);
 if(GetWorld()->LineTraceTestByChannel(From,Player->GetActorLocation(),ECC_Visibility,Query)) return;
 AttackTimer=FMath::Clamp(2.f-Profile.Dexterity*.028f,1.15f,1.95f);
 Stamina-=10.f;
 if(bRanged)
 {
  Mana-=14.f;
  Player->ShowSpell(From,Player->GetActorLocation(),Profile.Element,Profile.Color);
 }
 Player->ReceiveCombatHit(Profile.Weapon,Profile.Dexterity,bRanged?Profile.Element:ECryptDamage::Physical);
}

bool ACryptMonster::TakeHit(float Rating,float Dexterity,ECryptDamage Element)
{
 if(Health<=0.f) return false;
 const auto& Profile=Stats();
 if(Element==ECryptDamage::Physical && FMath::FRand()>CryptCombat::HitChance(Dexterity,Profile.Dexterity)) return false;
 const float Multiplier=Element==ECryptDamage::Fire?Profile.Fire:Element==ECryptDamage::Storm?Profile.Storm:1.f;
 Health=FMath::Max(0.f,Health-CryptCombat::Damage(Rating,Profile.Armor,Element)*Multiplier);
 if(Health>0.f) return true;
 if(bRoadRaider)
  if(auto* Hero=Cast<ACryptHero>(UGameplayStatics::GetPlayerPawn(this,0))) Hero->DefeatedRoadRaider();
 RestoreState(0.f,GetActorLocation());
 if(Dungeon!=INDEX_NONE)
  if(auto* Hero=Cast<ACryptHero>(UGameplayStatics::GetPlayerPawn(this,0))) Hero->CheckDungeonClear(Dungeon);
 return true;
}

void ACryptMonster::RestoreState(float SavedHealth,const FVector& Location)
{
 Health=FMath::Clamp(SavedHealth,0.f,Stats().Health);
 SetActorLocation(Location,false,nullptr,ETeleportType::TeleportPhysics);
 const bool bAlive=Health>0.f;
 SetActorHiddenInGame(!bAlive);
 SetActorEnableCollision(bAlive);
 SetActorTickEnabled(bAlive);
}
