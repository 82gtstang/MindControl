#include "CryptGameMode.h"
#include "CryptHero.h"
#include "CryptMonster.h"
#include "CryptMobileHUD.h"
#include "CryptTownActor.h"
#include "CryptWorldLayout.h"
#include "ProceduralMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Engine.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
namespace
{
 float TerrainHeight(float X,float Y)
 {
  const float South=FMath::SmoothStep(1100.f,2300.f,-Y);
  const float WestHill=660.f*FMath::Exp(-FMath::Square((X+3250.f)/1700.f)-FMath::Square((Y+2800.f)/1100.f));
  const float EastHill=550.f*FMath::Exp(-FMath::Square((X-3400.f)/1500.f)-FMath::Square((Y+2950.f)/1200.f));
  const float Valley=380.f*FMath::Exp(-FMath::Square((X-350.f)/1950.f)-FMath::Square((Y+2750.f)/1300.f));
  const float North=FMath::SmoothStep(1000.f,2700.f,Y)
   * (350.f*FMath::Exp(-FMath::Square((X+3300.f)/1450.f))+
     550.f*FMath::Exp(-FMath::Square((X-4375.f)/1100.f)));
  float Height=-20.f+South*(WestHill+EastHill-Valley)+North;
  const float Edge=FMath::Max(FMath::Abs(X)/5600.f,FMath::Abs(Y)/5100.f);
  Height+=760.f*FMath::SmoothStep(.96f,1.16f,Edge);
  for(const auto& River : CryptWorld::Rivers)
  {
   if(Y<River.Start || Y>River.End) continue;
   const float Bank=1.f-FMath::SmoothStep(River.Width*.84f,River.Width*1.75f,FMath::Abs(X-River.X(Y)));
   const float EndBlend=FMath::SmoothStep(0.f,180.f,FMath::Min(Y-River.Start,River.End-Y));
   Height=FMath::Lerp(Height,River.Height(Y)-58.f,Bank*EndBlend);
  }
  return Height;
 }

 UProceduralMeshComponent* CreateSurface(UWorld* World,const TCHAR* Name,bool bCollision)
 {
  AActor* Ground=World->SpawnActor<AActor>();
  if(!Ground) return nullptr;
  auto* Surface=NewObject<UProceduralMeshComponent>(Ground,FName(Name));
  Ground->AddInstanceComponent(Surface);
  Surface->SetMobility(EComponentMobility::Static);
  Ground->SetRootComponent(Surface);
  Surface->RegisterComponent();
  Surface->SetCollisionProfileName(bCollision?TEXT("BlockAll"):TEXT("NoCollision"));
  Surface->bUseComplexAsSimpleCollision=bCollision;
  return Surface;
 }

 bool IsDirt(float X,float Y)
 {
  if(X>-3500.f && X<3100.f && FMath::Abs(Y+1650.f+180.f*FMath::Sin(X/860.f))<125.f)
   return true;
  for(const auto& River : CryptWorld::Rivers)
   if(Y>River.Start && Y<River.End && FMath::Abs(X-River.X(Y))<River.Width*1.45f)
    return true;
  return false;
 }

 void CreateTerrain(UWorld* World,UMaterialInterface* Grass,UMaterialInterface* Dirt)
 {
  auto* Surface=CreateSurface(World,TEXT("GroundSurface"),true);
  if(!Surface) return;
  constexpr int32 Columns=132;
  constexpr int32 Rows=120;
  constexpr int32 Stride=Columns+1;
  TArray<FVector> Vertices,Normals;
  TArray<FVector2D> UVs;
  TArray<FProcMeshTangent> Tangents;
  TArray<int32> Triangles,DirtTriangles;
  Vertices.Reserve(Stride*(Rows+1));
  Normals.Reserve(Stride*(Rows+1));
  UVs.Reserve(Stride*(Rows+1));
  Tangents.Reserve(Stride*(Rows+1));
  Triangles.Reserve(Columns*Rows*6);
  DirtTriangles.Reserve(Columns*Rows);
  for(int32 Row=0;Row<=Rows;++Row)
  {
   const float Y=-6000.f+100.f*Row;
   for(int32 Column=0;Column<=Columns;++Column)
   {
    const float X=-6600.f+100.f*Column;
    const float Dx=(TerrainHeight(X+5,Y)-TerrainHeight(X-5,Y))*.1f;
    const float Dy=(TerrainHeight(X,Y+5)-TerrainHeight(X,Y-5))*.1f;
    Vertices.Emplace(X,Y,TerrainHeight(X,Y));
    Normals.Add(FVector(-Dx,-Dy,1).GetSafeNormal());
    Tangents.Emplace(FVector(1,0,Dx).GetSafeNormal(),false);
    UVs.Emplace(float(Column)/Columns,float(Row)/Rows);
    if(Row<Rows && Column<Columns)
    {
     const int32 A=Row*Stride+Column;
     const int32 B=A+1;
     const int32 C=A+Stride;
     const int32 D=C+1;
     auto& Section=(Dirt && IsDirt(X+50.f,Y+50.f))?DirtTriangles:Triangles;
     Section.Append({A,C,B,B,C,D});
    }
   }
  }
  Surface->CreateMeshSection_LinearColor(0,Vertices,Triangles,Normals,UVs,{},Tangents,true);
  if(Grass) Surface->SetMaterial(0,Grass);
  if(!DirtTriangles.IsEmpty())
  {
   Surface->CreateMeshSection_LinearColor(1,Vertices,DirtTriangles,Normals,UVs,{},Tangents,true);
   Surface->SetMaterial(1,Dirt);
  }
 }

 void CreateWater(UWorld* World,UMaterialInterface* RiverMaterial,UMaterialInterface* FallsMaterial)
 {
  if(!RiverMaterial || !FallsMaterial) return;
  for(const auto& River : CryptWorld::Rivers)
  {
   auto* Surface=CreateSurface(World,TEXT("RiverSurface"),false);
   if(!Surface) continue;
   constexpr int32 Samples=120;
   TArray<FVector> Vertices,Normals;
   TArray<FVector2D> UVs;
   TArray<FProcMeshTangent> Tangents;
   TArray<int32> Stream,Falls;
   for(int32 Index=0;Index<=Samples;++Index)
   {
    const float T=float(Index)/Samples;
    const float Y=FMath::Lerp(River.Start+150.f,River.End-150.f,T);
    const float X=River.X(Y);
    const float Z=River.Height(Y);
    const float HalfWidth=River.Width*.87f*FMath::Lerp(.40f,1.f,FMath::SmoothStep(0.f,.07f,FMath::Min(T,1.f-T)));
    const float Slope=(River.Height(Y+5)-River.Height(Y-5))*.1f;
    for(int32 Side=0;Side<2;++Side)
    {
     Vertices.Emplace(X+(Side==0?-HalfWidth:HalfWidth),Y,Z);
     Normals.Add(FVector(0,-Slope,1).GetSafeNormal());
     UVs.Emplace(Side,(River.End-Y)/220.f);
     Tangents.Emplace(FVector::ForwardVector,false);
    }
    if(Index<Samples)
    {
     auto& Section=(River.bFalls && T>=.46f && T<=.54f)?Falls:Stream;
     const int32 A=Index*2;
     Section.Append({A,A+2,A+1,A+1,A+2,A+3});
    }
   }
   Surface->CreateMeshSection_LinearColor(0,Vertices,Stream,Normals,UVs,{},Tangents,false);
   Surface->SetMaterial(0,RiverMaterial);
   if(!Falls.IsEmpty())
   {
    Surface->CreateMeshSection_LinearColor(1,Vertices,Falls,Normals,UVs,{},Tangents,false);
    Surface->SetMaterial(1,FallsMaterial);
   }
  }
 }
}

ACryptGameMode::ACryptGameMode() { DefaultPawnClass = ACryptHero::StaticClass(); HUDClass = ACryptMobileHUD::StaticClass(); }
void ACryptGameMode::PlaceBlock(const FVector& Location,const FVector& Scale,const FRotator& Rotation,UMaterialInterface* Material,UStaticMesh* Mesh)
{
 UWorld* World=GetWorld(); if(!World) return;
 AStaticMeshActor* Block=World->SpawnActor<AStaticMeshActor>(Location,Rotation);
 if(!Block) return;
 if(!Mesh) Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
 Block->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
 Block->GetStaticMeshComponent()->SetStaticMesh(Mesh);
 if(Material) Block->GetStaticMeshComponent()->SetMaterial(0,Material);
 Block->SetActorScale3D(Scale);
 Block->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
}
void ACryptGameMode::BeginPlay()
{
 Super::BeginPlay();
 // World units are centimeters; cubes are 100 cm per side.
 UMaterialInterface* Grass=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Environment/MindControl/M_Grass.M_Grass"));
 UMaterialInterface* Dirt=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Environment/MindControl/M_Dirt.M_Dirt"));
 UMaterialInterface* Cobble=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Environment/MindControl/M_Cobblestone.M_Cobblestone"));
 UMaterialInterface* River=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Environment/MindControl/M_River.M_River"));
 UMaterialInterface* Falls=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Environment/MindControl/M_Waterfall.M_Waterfall"));
 CreateTerrain(GetWorld(),Grass,Dirt);
 CreateWater(GetWorld(),River,Falls);
 // Keep movement inside the finished terrain while the surrounding hills continue past the boundary.
 for(const float Side : {-1.f,1.f})
 {
  for(const bool bAlongX : {false,true})
  {
   auto* Boundary=GetWorld()->SpawnActor<AStaticMeshActor>(
     FVector(bAlongX?0.f:Side*6020.f,bAlongX?Side*5480.f:0.f,375),FRotator::ZeroRotator);
   if(!Boundary) continue;
   auto* Collision=Boundary->GetStaticMeshComponent();
   Collision->SetMobility(EComponentMobility::Movable);
   Collision->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
   Collision->SetCollisionProfileName(TEXT("BlockAll"));
   Boundary->SetActorScale3D(bAlongX?FVector(125,1,28):FVector(1,115,28));
   Boundary->SetActorHiddenInGame(true);
   Collision->SetMobility(EComponentMobility::Static);
  }
 }
 UMaterialInterface* Stone=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Environment/MindControl/M_CastleStone.M_CastleStone"));
 PlaceBlock(FVector(0,0,-8),FVector(110,4,.12),FRotator::ZeroRotator,Cobble?Cobble:Stone); // main road
 // Wilderness stones and ruined pillars beside the approach.
 for(int32 i=0;i<10;++i)
 {
  const float X=-3100+i*420.f;
  const float Y=(i%2==0?1.f:-1.f)*(600.f+(i%3)*220.f);
  PlaceBlock(FVector(X,Y,75),FVector(1.2f,1.2f,2.5f+(i%3)),FRotator::ZeroRotator,Stone);
 }
 // Six houses in two rows, leaving the road unobstructed.
 for(int32 i=0;i<6;++i)
 {
  const float X=750+(i/2)*650.f;
  const float Y=(i%2==0?850.f:-850.f);
  PlaceBlock(FVector(X,Y,160),FVector(4.0f,4.2f,3.2f),FRotator::ZeroRotator,Stone);
  PlaceBlock(FVector(X,Y,340),FVector(4.8f,4.8f,.45f));
 }
 // Castle masonry uses world-space projection across the different wall sizes.
 UStaticMesh* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
 const auto Masonry=[this,Stone](const FVector& Location,const FVector& Scale,const FRotator& Rotation=FRotator::ZeroRotator,UStaticMesh* Mesh=nullptr)
 {
  PlaceBlock(Location,Scale,Rotation,Stone,Mesh);
 };
 for(const float X : {3800.f,5100.f})
 {
  for(const float Y : {-1050.f,1050.f})
  {
   Masonry(FVector(X,Y,330),FVector(2.8f,2.8f,7.f),FRotator::ZeroRotator,Cylinder);
   Masonry(FVector(X,Y,655),FVector(3.15f,3.15f,.35f),FRotator::ZeroRotator,Cylinder);
   for(int32 Segment=0;Segment<8;++Segment)
   {
    const float Yaw=45.f*Segment;
    const float Angle=FMath::DegreesToRadians(Yaw);
    Masonry(FVector(X+135.f*FMath::Cos(Angle),Y+135.f*FMath::Sin(Angle),710),
      FVector(.42f,.62f,.85f),FRotator(0,Yaw,0));
   }
  }
 }
 for(const float Y : {-1050.f,1050.f})
 {
  Masonry(FVector(4450,Y,290),FVector(13,1.2f,6));
  Masonry(FVector(4450,Y,590),FVector(13,1.45f,.28f));
 }
 Masonry(FVector(5100,0,290),FVector(1.2f,21,6));
 Masonry(FVector(4600,0,560),FVector(6,6,12));
 Masonry(FVector(4600,0,1180),FVector(7,7,.55f));
 for(int32 Segment=0;Segment<7;++Segment)
 {
  const float Offset=-285.f+Segment*95.f;
  for(const float Edge : {-305.f,305.f})
  {
   Masonry(FVector(4600+Edge,Offset,1230),FVector(.65f,.55f,.75f));
   if(Segment>0 && Segment<6)
    Masonry(FVector(4600+Offset,Edge,1230),FVector(.55f,.65f,.75f));
  }
 }
 // Three monsters stand between the starting point and the town.
 for(int32 i=0;i<3;++i)
 {
  const FVector P(-1650.f+i*680.f,(i%2==0?220.f:-260.f),120.f);
  if(ACryptMonster* Raider=GetWorld()->SpawnActor<ACryptMonster>(P,FRotator::ZeroRotator))
  {
   Raider->bRoadRaider=true;
   Raider->SaveId=i;
  }
  else
   UE_LOG(LogTemp,Warning,TEXT("MindControl: failed to spawn monster %d"),i);
 }
 // Open-air stone dungeon: a walk-in ruin with pillars, treasure marker and guardians.
 // Raise dungeon paving above terrain (terrain top = -20 cm) to avoid coplanar Z-fighting.
 PlaceBlock(FVector(1000,3200,-57),FVector(36,30,1),FRotator::ZeroRotator,Stone);
 for(int32 i=0;i<8;++i)
 {
  const float X=-600.f+(i%4)*950.f;
  const float Y=1800.f+(i/4)*2800.f;
  PlaceBlock(FVector(X,Y,160),FVector(1.5f,1.5f,4.5f));
 }
 PlaceBlock(FVector(1000,4650,180),FVector(20,1.5f,5));
 PlaceBlock(FVector(-700,3200,180),FVector(1.5f,30,5));
 PlaceBlock(FVector(2700,3200,180),FVector(1.5f,30,5));
 for(int32 i=0;i<5;++i)
  if(ACryptMonster* Guardian=GetWorld()->SpawnActor<ACryptMonster>(FVector(200+i*380.f,3000.f+(i%2)*300.f,120.f),FRotator::ZeroRotator))
   Guardian->SaveId=100+i;
 // Roadside timber fences, lantern posts, rubble, and a village square.
 for(int32 i=0;i<26;++i)
 {
  const float X=-2700.f+i*275.f;
  for(int32 Side=-1;Side<=1;Side+=2)
  {
   const float Y=Side*420.f;
   PlaceBlock(FVector(X,Y,35),FVector(.22f,.22f,1.35f));
   PlaceBlock(FVector(X+135.f,Y,82),FVector(2.7f,.12f,.15f));
  }
 }
 for(int32 i=0;i<12;++i)
 {
  const float X=-2300.f+i*540.f;
  const float Y=(i%2==0?1.f:-1.f)*490.f;
  PlaceBlock(FVector(X,Y,135),FVector(.18f,.18f,3.f));
  PlaceBlock(FVector(X,Y,290),FVector(.55f,.55f,.45f));
 }
 // House doorways, chimneys, shutters, and roof beams.
 for(int32 i=0;i<6;++i)
 {
  const float X=750.f+(i/2)*650.f;
  const float Y=(i%2==0?850.f:-850.f);
  const float Front=Y+(i%2==0?-220.f:220.f);
  PlaceBlock(FVector(X,Front,95),FVector(1.15f,.18f,2.0f));
  PlaceBlock(FVector(X+155.f,Y,540),FVector(.65f,.65f,3.2f));
  for(int32 j=-1;j<=1;j+=2)
  {
   PlaceBlock(FVector(X+j*145.f,Front,240),FVector(.7f,.22f,.85f));
   PlaceBlock(FVector(X+j*145.f,Front+(i%2==0?-16.f:16.f),240),FVector(.08f,.28f,.85f));
  }
 }
 // A cobbled village square with market stalls and stone benches.
 // Square paving top = +19 cm, above road top = -2 cm; avoid near-coplanar surfaces.
 PlaceBlock(FVector(2400,0,12),FVector(22,18,.14f),FRotator::ZeroRotator,Cobble?Cobble:Stone);
 for(int32 i=0;i<9;++i)
 {
  const float X=1600.f+(i%3)*750.f;
  const float Y=-650.f+(i/3)*650.f;
  PlaceBlock(FVector(X,Y,18),FVector(1.4f,1.4f,.22f),FRotator::ZeroRotator,Cobble?Cobble:Stone);
 }
 for(int32 i=0;i<4;++i)
 {
  const float X=1550.f+i*580.f;
  PlaceBlock(FVector(X,1000,110),FVector(3.1f,1.5f,.25f));
  for(int32 j=-1;j<=1;j+=2)
   PlaceBlock(FVector(X+j*125.f,1000,60),FVector(.22f,.22f,1.25f));
 }
 // Crenellations, gate piers and voussoirs framing the open road.
 for(int32 i=0;i<12;++i)
 {
  const float X=3850.f+i*105.f;
  Masonry(FVector(X,-1050,640),FVector(.66f,1.3f,.85f));
  Masonry(FVector(X,1050,640),FVector(.66f,1.3f,.85f));
 }
 for(const float Y : {-260.f,260.f})
 {
  Masonry(FVector(3790,Y,190),FVector(1.1f,1.1f,4.2f));
  Masonry(FVector(3790,Y,400),FVector(1.25f,1.25f,.32f));
 }
 for(int32 Segment=0;Segment<=12;++Segment)
 {
  const float AngleDegrees=Segment*15.f;
  const float Angle=FMath::DegreesToRadians(AngleDegrees);
  Masonry(FVector(3790,260.f*FMath::Cos(Angle),400.f+260.f*FMath::Sin(Angle)),
    FVector(1.2f,.74f,.71f),FRotator(0,0,90.f-AngleDegrees));
 }
 // Ruined dungeon chambers with partial partitions and stone rubble.
 for(int32 i=0;i<5;++i)
 {
  const float X=-250.f+i*550.f;
  PlaceBlock(FVector(X,2600,90),FVector(.8f,9.f,2.7f));
  PlaceBlock(FVector(X,3750,90),FVector(.8f,9.f,2.7f));
 }
 for(int32 i=0;i<22;++i)
 {
  const float X=-450.f+(i*317%2750);
  const float Y=1900.f+(i*491%2400);
  PlaceBlock(FVector(X,Y,12),FVector(.35f+(i%3)*.2f,.5f,.35f));
 }
 // Place the townspeople along the square, then put their objectives on accessible terrain.
 const auto SpawnQuestActor=[this](int32 Quest,bool bGiver,FVector Location,int32 ObjectiveId=INDEX_NONE)
 {
  if(ACryptTownActor* Actor=GetWorld()->SpawnActor<ACryptTownActor>(Location,FRotator(0,180,0)))
  {
   Actor->Configure(Quest,bGiver);
   Actor->ObjectiveId=ObjectiveId;
  }
 };
 for(int32 Index=0;Index<5;++Index)
  SpawnQuestActor(Index,true,FVector(1450.f+400.f*Index,185.f,19.f));

 const auto SpawnTerrainObjective=[&](int32 Quest,float X,float Y,int32 ObjectiveId)
 {
  SpawnQuestActor(Quest,false,FVector(X,Y,TerrainHeight(X,Y)),ObjectiveId);
 };
 SpawnTerrainObjective(1,-430.f,-2000.f,10);
 SpawnTerrainObjective(1,250.f,-2360.f,11);
 SpawnTerrainObjective(1,720.f,-2750.f,12);
 SpawnQuestActor(2,false,FVector(1020,3110,-7),20);
 SpawnTerrainObjective(3,-3300.f,-2780.f,30);
 SpawnTerrainObjective(4,2450.f,-2330.f,40);
}
