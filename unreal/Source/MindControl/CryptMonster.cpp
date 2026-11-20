#include "CryptMonster.h"
#include "CryptHero.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
ACryptMonster::ACryptMonster()
{
 PrimaryActorTick.bCanEverTick=true;
 Collision=CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
 RootComponent=Collision;
 Collision->InitSphereRadius(85.f);
 Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MonsterBody"));
 Body->SetupAttachment(RootComponent);
 Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Body->SetRelativeScale3D(FVector(1.2f,1.2f,1.8f));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
 if(Sphere.Succeeded()) Body->SetStaticMesh(Sphere.Object);
}
void ACryptMonster::Tick(float Dt)
{
 Super::Tick(Dt);
 APawn* Player=UGameplayStatics::GetPlayerPawn(this,0);
 if(!Player) return;
 FVector Offset=Player->GetActorLocation()-GetActorLocation();
 Offset.Z=0;
 if(Offset.Size()>180.f && Offset.Size()<1300.f)
 {
  SetActorLocation(GetActorLocation()+Offset.GetSafeNormal()*FMath::Min(130.f*Dt,Offset.Size()-180.f),true);
  SetActorRotation(Offset.Rotation());
 }
}
void ACryptMonster::TakeHit(float Damage)
{
 if(Health<=0.f) return;
 Health-=Damage;
 if(Health>0.f) return;
 if(bRoadRaider)
 {
  if(ACryptHero* Hero=Cast<ACryptHero>(UGameplayStatics::GetPlayerPawn(this,0)))
   Hero->DefeatedRoadRaider();
 }
 Destroy();
}
