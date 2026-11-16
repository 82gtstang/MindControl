#include "CryptGameMode.h"
#include "CryptHero.h"
#include "CryptMonster.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
ACryptGameMode::ACryptGameMode() { DefaultPawnClass = ACryptHero::StaticClass(); }
void ACryptGameMode::PlaceBlock(const FVector& Location,const FVector& Scale,const FRotator& Rotation)
{
 UWorld* World=GetWorld(); if(!World) return;
 AStaticMeshActor* Block=World->SpawnActor<AStaticMeshActor>(Location,Rotation);
 if(!Block) return;
 UStaticMesh* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
 Block->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
 Block->GetStaticMeshComponent()->SetStaticMesh(Cube);
 Block->SetActorScale3D(Scale);
 Block->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
}
void ACryptGameMode::BeginPlay()
{
 Super::BeginPlay();
 // Keep startup breadcrumbs in the app's writable saved directory.
 const FString DiagnosticPath=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("MindControlStartup.txt"));
 FString Previous;
 FFileHelper::LoadFileToString(Previous,*DiagnosticPath);
 const bool bPreviousStartupInterrupted=Previous.StartsWith(TEXT("STARTING"));
 FFileHelper::SaveStringToFile(TEXT("STARTING: world generation"),*DiagnosticPath);
 UE_LOG(LogTemp,Warning,TEXT("MindControl startup: previous incomplete=%d"),bPreviousStartupInterrupted?1:0);
 if(bPreviousStartupInterrupted)
 {
  UE_LOG(LogTemp,Error,TEXT("MindControl SAFE MODE: skipping procedural world after incomplete startup"));
  if(GEngine) GEngine->AddOnScreenDebugMessage(-1,30.f,FColor::Yellow,TEXT("MIND CONTROL SAFE MODE - screenshot this message"));
  FFileHelper::SaveStringToFile(TEXT("SAFE MODE: previous startup did not complete"),*DiagnosticPath);
  return;
 }
 if(GEngine) GEngine->AddOnScreenDebugMessage(-1,15.f,FColor::Green,TEXT("Mind Control: loading town and monsters"));
 // World units are centimeters; cubes are 100 cm per side.
 PlaceBlock(FVector(0,0,-70),FVector(110,70,1)); // walkable ground
 PlaceBlock(FVector(0,0,-12),FVector(110,4,.12)); // main road
 // Wilderness stones and ruined pillars beside the approach.
 for(int32 i=0;i<10;++i)
 {
  const float X=-3100+i*420.f;
  const float Y=(i%2==0?1.f:-1.f)*(600.f+(i%3)*220.f);
  PlaceBlock(FVector(X,Y,75),FVector(1.2f,1.2f,2.5f+(i%3)));
 }
 // Six houses in two rows, leaving the road unobstructed.
 for(int32 i=0;i<6;++i)
 {
  const float X=750+(i/2)*650.f;
  const float Y=(i%2==0?850.f:-850.f);
  PlaceBlock(FVector(X,Y,160),FVector(4.0f,4.2f,3.2f));
  PlaceBlock(FVector(X,Y,340),FVector(4.8f,4.8f,.45f));
 }
 // Castle walls, central keep and four towers at the far end of town.
 PlaceBlock(FVector(3800,-1050,330),FVector(2,2,7));
 PlaceBlock(FVector(3800,1050,330),FVector(2,2,7));
 PlaceBlock(FVector(5100,-1050,330),FVector(2,2,7));
 PlaceBlock(FVector(5100,1050,330),FVector(2,2,7));
 PlaceBlock(FVector(4450,-1050,290),FVector(13,1.2f,6));
 PlaceBlock(FVector(4450,1050,290),FVector(13,1.2f,6));
 PlaceBlock(FVector(5100,0,290),FVector(1.2f,21,6));
 PlaceBlock(FVector(4600,0,560),FVector(6,6,12));
 PlaceBlock(FVector(4600,0,1180),FVector(7,7,1));
 // Three monsters stand between the starting point and the town.
 for(int32 i=0;i<3;++i)
 {
  const FVector P(-1650.f+i*680.f,(i%2==0?220.f:-260.f),120.f);
  if(!GetWorld()->SpawnActor<ACryptMonster>(P,FRotator::ZeroRotator))
   UE_LOG(LogTemp,Warning,TEXT("MindControl: failed to spawn monster %d"),i);
 }
 FFileHelper::SaveStringToFile(TEXT("READY: world generation completed"),*DiagnosticPath);
 UE_LOG(LogTemp,Display,TEXT("MindControl: world generation completed"));
 if(GEngine) GEngine->AddOnScreenDebugMessage(-1,15.f,FColor::Green,TEXT("Mind Control: town is ahead - follow the road"));
}
