#include "CryptGameMode.h"
#include "CryptHero.h"
#include "CryptMonster.h"
#include "CryptMobileHUD.h"
#include "CryptTownActor.h"
#include "CryptWorldLayout.h"
#include "CryptSettlements.h"
#include "CryptDungeons.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"
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
  const float CoreDistance=FMath::Max(FMath::Abs(X),FMath::Abs(Y));
  Height+=760.f*FMath::SmoothStep(.96f,1.16f,Edge)*(1.f-FMath::SmoothStep(6300.f,12200.f,CoreDistance));
  const float Wilder=FMath::SmoothStep(6600.f,12200.f,CoreDistance);
  const float Ridges=610.f*FMath::Sin((X+.32f*Y)/5900.f)+420.f*FMath::Cos((Y-.24f*X)/4600.f);
  const float Rolling=175.f*FMath::Sin(X/1960.f)*FMath::Cos(Y/2420.f);
  Height+=Wilder*(Ridges+Rolling+750.f);
  const float MapEdge=FMath::Max(FMath::Abs(X)/CryptWorld::HalfWorldX,FMath::Abs(Y)/CryptWorld::HalfWorldY);
  Height+=1800.f*FMath::SmoothStep(.94f,1.f,MapEdge);
  if(X<-6600.f)
  {
   const float ByTown=(1.f-FMath::SmoothStep(24900.f,27600.f,-X))*
      (1.f-FMath::SmoothStep(5850.f,8400.f,FMath::Abs(Y)));
   Height=FMath::Lerp(Height,-33.f,ByTown);
  }
  for(const auto& River : CryptWorld::Rivers)
  {
   if(Y<River.Start || Y>River.End) continue;
   const float Bank=1.f-FMath::SmoothStep(River.BedWidth(),River.BedWidth()+155.f,FMath::Abs(X-River.X(Y)));
   const float EndBlend=FMath::SmoothStep(0.f,180.f,FMath::Min(Y-River.Start,River.End-Y));
   Height=FMath::Lerp(Height,River.Height(Y)-95.f,Bank*EndBlend);
  }
  for(int32 I=0;I<CryptDungeons::Count;++I)
  {
   const FVector2D Point=CryptDungeons::EntrancePoints[I];
   const float Dx=(X-Point.X)/580.f, Dy=(Y-Point.Y)/610.f;
   if(Dx<=.15f || Dx>=2.3f || FMath::Abs(Dy)>2.f) continue;
   // Raise a shoulder behind the entrance and taper it into the existing terrain.
   const float Shape=FMath::Exp(-FMath::Square(Dx-.96f)*2.f-FMath::Square(Dy)*1.8f);
   Height+=(I==8?78.f:199.f)*Shape*FMath::SmoothStep(.15f,.72f,Dx)
    *(1.f-FMath::SmoothStep(1.8f,2.3f,Dx))*(1.f-FMath::SmoothStep(1.45f,2.f,FMath::Abs(Dy)));
  }
  const float WestRoad = FMath::SmoothStep(5500.f, 6520.f, -X)
   * (1.f - FMath::SmoothStep(235.f, 575.f, FMath::Abs(Y)));
  return FMath::Lerp(Height, -20.f, WestRoad);
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
  for(const auto& Entrance : CryptDungeons::EntrancePoints)
  {
   const float Along=X-Entrance.X;
   const float Across=Y-Entrance.Y;
   if(Along>-730.f && Along<165.f && FMath::Abs(Across-46.f*FMath::Sin(Along/190.f))
     < 118.f+28.f*FMath::Cos(Along/170.f)) return true;
  }
  if(X>-3500.f && X<3100.f && FMath::Abs(Y+1650.f+180.f*FMath::Sin(X/860.f))<125.f)
   return true;
  for(const auto& River : CryptWorld::Rivers)
   if(Y>River.Start && Y<River.End && FMath::Abs(X-River.X(Y))<River.BedWidth()+60.f)
    return true;
  return false;
 }

 void DisableTemplateGround(UWorld* World)
 {
  // The engine startup level can have its own floor. A terrain cutout only
  // removes our mesh triangles, so that original floor must not span the shaft.
  for(TActorIterator<AActor> It(World);It;++It)
  {
   AActor* Actor=*It;
   if(Actor->IsA<AStaticMeshActor>() || Actor->GetClass()->GetName().StartsWith(TEXT("Landscape")))
   {
    TArray<UPrimitiveComponent*> Primitives;
    Actor->GetComponents(Primitives);
    for(UPrimitiveComponent* Primitive : Primitives)
    {
     const auto& Bounds=Primitive->Bounds;
     const FVector Extent=Bounds.BoxExtent;
     const bool bLandscape=Actor->GetClass()->GetName().StartsWith(TEXT("Landscape"));
     if(!bLandscape && (Extent.X<500 || Extent.Y<500 || Extent.Z>220)) continue;
     for(int32 I=0;I<CryptDungeons::Count;++I)
     {
      const FVector Mouth=CryptDungeons::Entrance(I);
      const FVector Chamber=CryptDungeons::Room(I);
      const FBox Shaft(FVector(Mouth.X-20,Mouth.Y-CryptDungeons::RampHalfWidth,Chamber.Z),
                       FVector(Mouth.X+CryptDungeons::RampLength+20,Mouth.Y+CryptDungeons::RampHalfWidth,Mouth.Z+200));
      if(!Shaft.Intersect(Bounds.GetBox())) continue;
      Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
      Primitive->SetVisibility(false,true);
      break;
     }
    }
   }
  }
 }

 void CreateTerrain(UWorld* World,UMaterialInterface* Grass,UMaterialInterface* Dirt)
 {
  auto* Surface=CreateSurface(World,TEXT("GroundSurface"),true);
  if(!Surface) return;
  TArray<float> Eastings,Northings;
  for(float X=-CryptWorld::HalfWorldX;X<=CryptWorld::HalfWorldX;X+=1500.f) Eastings.Add(X);
  for(float Y=-CryptWorld::HalfWorldY;Y<=CryptWorld::HalfWorldY;Y+=1500.f) Northings.Add(Y);
  for(float X=-6600;X<=6600;X+=100) Eastings.AddUnique(X);
  for(float Y=-6000;Y<=6000;Y+=100) Northings.AddUnique(Y);
  Eastings.AddUnique(-24900);
  Northings.AddUnique(-5850);
  Northings.AddUnique(5850);
  for(const auto& P : CryptDungeons::EntrancePoints)
  {
   for(float X=P.X-300;X<=P.X+300;X+=100) Eastings.AddUnique(X);
   Eastings.AddUnique(P.X+CryptDungeons::RampLength);
   for(float Y=P.Y-300;Y<=P.Y+300;Y+=100) Northings.AddUnique(Y);
  }
  Eastings.Sort();
  Northings.Sort();
  const int32 Columns=Eastings.Num()-1;
  const int32 Rows=Northings.Num()-1;
  const int32 Stride=Columns+1;
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
   const float Y=Northings[Row];
   for(int32 Column=0;Column<=Columns;++Column)
   {
    const float X=Eastings[Column];
    const float Dx=(TerrainHeight(X+5,Y)-TerrainHeight(X-5,Y))*.1f;
    const float Dy=(TerrainHeight(X,Y+5)-TerrainHeight(X,Y-5))*.1f;
    Vertices.Emplace(X,Y,TerrainHeight(X,Y));
    Normals.Add(FVector(-Dx,-Dy,1).GetSafeNormal());
    Tangents.Emplace(FVector(1,0,Dx).GetSafeNormal(),false);
    UVs.Emplace(float(Column)/Columns,float(Row)/Rows);
    if(Row<Rows && Column<Columns)
    {
     const float TileX=(X+Eastings[Column+1])*.5f;
     const float TileY=(Y+Northings[Row+1])*.5f;
     if((TileX>-24900 && TileX<-6600 && FMath::Abs(TileY)<5850) ||
        CryptDungeons::IsRampCell(TileX,TileY)) continue;
     const int32 A=Row*Stride+Column;
     const int32 B=A+1;
     const int32 C=A+Stride;
     const int32 D=C+1;
     auto& Section=(Dirt && IsDirt(TileX,TileY))?DirtTriangles:Triangles;
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
    const float Y=FMath::Lerp(River.Start+190.f,River.End-190.f,T);
    const float X=River.X(Y);
    const float Z=River.Height(Y)+6.f;
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

double CryptDungeons::GetGroundHeight(double X,double Y)
{
 return TerrainHeight(X,Y);
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
 Block->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
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
 DisableTemplateGround(GetWorld());
 CreateTerrain(GetWorld(),Grass,Dirt);
 CreateWater(GetWorld(),River,Falls);
 // Keep movement inside the finished terrain while the surrounding hills continue past the boundary.
 for(const float Side : {-1.f,1.f})
 {
  for(const bool bAlongX : {false,true})
  {
   auto* Boundary=GetWorld()->SpawnActor<AStaticMeshActor>(
     FVector(bAlongX?0.f:Side*(CryptWorld::HalfWorldX-80),bAlongX?Side*(CryptWorld::HalfWorldY-80):0.f,3000),FRotator::ZeroRotator);
   if(!Boundary) continue;
   auto* Collision=Boundary->GetStaticMeshComponent();
   Collision->SetMobility(EComponentMobility::Movable);
   Collision->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
   Collision->SetCollisionProfileName(TEXT("BlockAll"));
   Boundary->SetActorScale3D(bAlongX?FVector(2*CryptWorld::HalfWorldX/100,1,130):FVector(1,2*CryptWorld::HalfWorldY/100,130));
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
 PlaceBlock(FVector(-6048,0,-24),FVector(11.04,4,.10),FRotator::ZeroRotator,Cobble?Cobble:Stone);
 CryptSettlements::Build(GetWorld(),Grass,Stone,Cobble?Cobble:Stone,Dirt?Dirt:Stone,-20.f);
 {
  using namespace CryptDungeons;
  auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
  auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
  const auto Accent=[&](FLinearColor Color) -> UMaterialInterface*
  {
   if(!Base) return Stone;
   auto* Material=UMaterialInstanceDynamic::Create(Base,this);
   Material->SetVectorParameterValue(TEXT("Color"),Color);
   return Material;
  };
  const auto Block=[&](FVector P,FVector Size,UMaterialInterface* Material)
  {
   PlaceBlock(P,Size/100.,FRotator::ZeroRotator,Material,Cube);
  };
  const auto Sign=[&](FVector Location,const TCHAR* Title,float Size)
  {
   auto* Actor=GetWorld()->SpawnActor<AActor>(Location,FRotator::ZeroRotator);
   if(!Actor) return;
   auto* Label=NewObject<UTextRenderComponent>(Actor);
   Actor->SetRootComponent(Label);
   Actor->AddInstanceComponent(Label);
   Label->RegisterComponent();
   Label->SetWorldLocationAndRotation(Location,FRotator(0,180,0));
   Label->SetHorizontalAlignment(EHTA_Center);
   Label->SetWorldSize(Size);
   Label->SetText(FText::FromString(Title));
  };
  auto* Boulder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
  auto* Trunk=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
  auto* Bark=Accent(FLinearColor(.099,.064,.038));
  auto* Pine=Accent(FLinearColor(.055,.115,.077));
  auto* Torch=Accent(FLinearColor(1,.32,.047));
  auto* Rune=Accent(FLinearColor(.39,.78,1));
  const auto SurfaceEntrance=[&](int32 Index,UMaterialInterface* Mark)
  {
   const FVector Origin=Entrance(Index);
   const bool bForest=Index==1 || Index==8 || Index==6;
   const bool bCutStone=Index==0 || Index==2 || Index==5 || Index==9;
   const bool bRunes=Index==4 || Index==9;
   const bool bSinkhole=Index==8;
   const TCHAR* MeshName=bSinkhole?TEXT("ForestBasin"):bRunes?TEXT("RuneGate"):
    bCutStone?TEXT("CliffTomb"):TEXT("CaveMouth");
   const FString MeshPath=FString::Printf(TEXT("/Game/Characters/Town/Entrances/%s.%s"),MeshName,MeshName);
   auto* Sculpt=LoadObject<UStaticMesh>(nullptr,*MeshPath);
   if(Sculpt)
   {
    if(auto* RockFace=GetWorld()->SpawnActor<AStaticMeshActor>(Origin,FRotator(0,90,0)))
    {
     auto* Visual=RockFace->GetStaticMeshComponent();
     Visual->SetMobility(EComponentMobility::Movable);
     Visual->SetStaticMesh(Sculpt);
     Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
     if(Stone) Visual->SetMaterial(0,Stone);
     Visual->SetMobility(EComponentMobility::Static);
    }
   }
   FRandomStream Random(317+Index*97);
   const auto Rock=[&](FVector Offset,FVector Size)
   {
    PlaceBlock(Origin+Offset,Size/100.,FRotator(Random.FRandRange(-14,14),
       Random.FRandRange(-180,180),Random.FRandRange(-18,18)),Stone,Boulder);
   };
   const auto Tree=[&](double DX,double DY)
   {
    const double X=Origin.X+DX, Y=Origin.Y+DY;
    const double Ground=TerrainHeight(X,Y);
    const float H=Random.FRandRange(580,900);
    PlaceBlock(FVector(X,Y,Ground+H*.5),FVector(.45,.45,H/100.),FRotator(3,0,0),Bark,Trunk);
    for(int32 Tier=0;Tier<3;++Tier)
    {
     const float Width=300.f-Tier*63.f;
     PlaceBlock(FVector(X,Y,Ground+H*(.67+Tier*.139)),FVector(Width,Width*.86,225)/100.,
       FRotator(0,Tier*41,0),Pine,Boulder);
    }
   };

   if(bForest)
   {
    for(int32 T=0;T<12;++T)
    {
     const double Side=T%2?1.:-1.;
     Tree(Random.FRandRange(-570,830),Side*Random.FRandRange(370,850));
    }
   }
   for(int32 R=0;R<14;++R)
   {
    const double Side=R%2?1.:-1.;
    const double OffsetX=Random.FRandRange(-425,810);
    const double OffsetY=Side*Random.FRandRange(260,685);
    const double Ground=TerrainHeight(Origin.X+OffsetX,Origin.Y+OffsetY);
    const float Size=Random.FRandRange(65,225);
    Rock(FVector(OffsetX,OffsetY,Ground-Origin.Z+Size*.26),FVector(Size*1.22,Size,Size*.75));
   }

   if(bSinkhole)
   {
    // Arc around the far side, with a walkable gap on the west approach.
    for(int32 R=0;R<11;++R)
    {
     const double Angle=FMath::DegreesToRadians(-128.+R*25.6);
     if(!Sculpt) Rock(FVector(30+FMath::Cos(Angle)*167,FMath::Sin(Angle)*188,28),
      FVector(138,128,109));
    }

   }
   else if(bCutStone)
   {
    // Recessed lintel and stepped jambs on a textured escarpment.
    for(const double Side : {-1.,1.})
    {
     Rock(FVector(190,Side*307,151),FVector(670,364,439));
     Rock(FVector(230,Side*457,250),FVector(583,285,438));
     Block(Origin+FVector(-3,Side*146,121),FVector(126,61,255),Stone);
     if(!Sculpt) Block(Origin+FVector(-37,Side*187,109),FVector(104,35,229),Stone);
     Block(Origin+FVector(-17,Side*151,18),FVector(151,85,41),Stone);
    }
    Block(Origin+FVector(-3,0,263),FVector(160,354,52),Stone);
    Block(Origin+FVector(-18,0,297),FVector(164,421,29),Stone);
    Rock(FVector(190,0,353),FVector(673,752,229));
   }
   else
   {
    // Layered boulders form an irregular arch without filling its opening.
    for(const double Side : {-1.,1.})
    {
     Rock(FVector(131,Side*236,123),FVector(462,235,327));
     Rock(FVector(163,Side*346,157),FVector(469,295,360));
     Rock(FVector(209,Side*209,258),FVector(449,195,157));
    }
    Rock(FVector(138,0,322),FVector(448,490,169));
    Rock(FVector(362,0,388),FVector(444,565,166));
   }

   if(!bSinkhole)
   {
    for(int32 Step=0;Step<3;++Step)
    {
     const double X=Origin.X-193+Step*69;
     const double Ground=TerrainHeight(X,Origin.Y);
     Block(FVector(X,Origin.Y,Ground+1),FVector(62,220,20),Cobble?Cobble:Stone);
    }
   }

   for(const double Side : {-1.,1.})
   {
    if(bRunes)
    {
     // Original angular glyphs inset along each edge, visible from the trail.
     for(int32 Row=0;Row<4;++Row)
     {
      const FVector Glyph=Origin+FVector(-114,Side*153,77+Row*43);
      Block(Glyph,FVector(3,5,27),Rune);
      PlaceBlock(Glyph+FVector(0,Side*8,4),FVector(.03,.22,.035),FRotator(0,0,Side*36),Rune,Cube);
      PlaceBlock(Glyph+FVector(0,-Side*5,-7),FVector(.03,.18,.035),FRotator(0,0,-Side*43),Rune,Cube);
     }
    }
    else if(!bSinkhole)
    {
     const FVector Brazier=Origin+FVector(-82,Side*194,79);
     PlaceBlock(Brazier,FVector(.19,.19,.91),FRotator::ZeroRotator,Bark,Trunk);
     PlaceBlock(Brazier+FVector(0,0,51),FVector(.32,.32,.24),FRotator::ZeroRotator,Stone,Boulder);
     PlaceBlock(Brazier+FVector(0,0,70),FVector(.13,.13,.39),FRotator(13,0,0),Torch,Boulder);
     if(auto* Light=GetWorld()->SpawnActor<APointLight>(Brazier+FVector(-25,0,78),FRotator::ZeroRotator))
     {
      auto* Lamp=Light->FindComponentByClass<UPointLightComponent>();
      if(!Lamp) continue;
      Lamp->SetMobility(EComponentMobility::Movable);
      Lamp->SetIntensity(1200);
      Lamp->SetLightColor(FLinearColor(1.f,.42f,.18f));
      Lamp->SetAttenuationRadius(285);
      Lamp->SetCastShadows(false);
     }
    }
   }
   const double SignX=Origin.X-225;
   const double SignY=Origin.Y-193;
   const FVector SignPost(SignX,SignY,TerrainHeight(SignX,SignY));
   Block(SignPost+FVector(0,0,57),FVector(12,12,114),Bark);
   Block(SignPost+FVector(0,0,115),FVector(9,224,34),Mark);
   Sign(SignPost+FVector(-9,0,115),Chambers[Index].Name,21);
  };
  const auto PassageLamp=[&](FVector Position,FLinearColor Tint)
  {
   if(auto* Light=GetWorld()->SpawnActor<APointLight>(Position,FRotator::ZeroRotator))
   {
    if(auto* Lamp=Light->FindComponentByClass<UPointLightComponent>())
    {
     Lamp->SetMobility(EComponentMobility::Movable);
     Lamp->SetIntensity(4950);
     Lamp->SetLightColor(Tint);
     Lamp->SetAttenuationRadius(1875);
     Lamp->SetCastShadows(false);
    }
   }
  };
  for(int32 I=0;I<Count;++I)
  {
   const auto& Definition=Chambers[I];
   auto* Mark=Accent(Definition.Accent);
   SurfaceEntrance(I,Mark);
   const FVector Center=Room(I);
   const FVector Mouth=Entrance(I);
   const double East=Mouth.X+RampLength;
   const double HalfWidth=RampHalfWidth;
   // The thick stone slab uses the engine cube's simple collision. Align its
   // top face to both thresholds so the hero can descend and climb back out.
   constexpr double SlabThickness=44.;
   const FVector End(East,Mouth.Y,Center.Z);
   const FVector Run=End-Mouth;
   const FRotator Pitch=Run.Rotation();
   const FVector SlabCenter=(Mouth+End)*.5-Pitch.RotateVector(FVector(0,0,SlabThickness*.5));
   PlaceBlock(SlabCenter,FVector(Run.Size(),2*HalfWidth,SlabThickness)/100.,Pitch,Stone,Cube);

   for(int32 Section=0;Section<10;++Section)
   {
    const double Fraction=(Section+.5)/10.;
    const double X=Mouth.X+RampLength*Fraction;
    const double Bottom=FMath::Lerp(Mouth.Z,Center.Z,(Section+1)/10.)-30;
    for(double Side : {-1.,1.})
    {
     const double Y=Mouth.Y+Side*(HalfWidth+10);
     const double Top=FMath::Max(Bottom+110,GetGroundHeight(X,Y)+40);
     Block(FVector(X,Y,(Bottom+Top)*.5),FVector(RampLength/10.,24,Top-Bottom),Stone);
    }
   }

   Block(Center+FVector(0,0,-25),FVector(2*RoomHalfLength,2*RoomHalfWidth,50),Stone);
   Block(Center+FVector(0,0,CeilingHeight+22),FVector(2*RoomHalfLength,2*RoomHalfWidth,44),Stone);
   for(double Side : {-1.,1.})
   {
    Block(Center+FVector(0,Side*RoomHalfWidth,CeilingHeight*.5),FVector(2*RoomHalfLength,35,CeilingHeight),Stone);
    const double Wing=RoomHalfWidth-HalfWidth;
    Block(FVector(East+6,Center.Y+Side*(HalfWidth+Wing*.5),Center.Z+CeilingHeight*.5),FVector(22,Wing,CeilingHeight),Stone);
   }
   Block(Center+FVector(RoomHalfLength,0,CeilingHeight*.5),FVector(32,2*RoomHalfWidth,CeilingHeight),Stone);
   for(double Fraction : {.20,.52,.84})
   {
    const FVector Point=FMath::Lerp(Mouth,End,Fraction);
    PassageLamp(Point+FVector(0,0,310),FLinearColor(1.f,.58f,.32f));
   }
   for(int32 Row=0;Row<3;++Row)
   {
    const double Y=(Row-1)*RoomHalfWidth*.72;
    for(int32 Bay=0;Bay<8;++Bay)
    {
     const double X=((Bay+.5)/4.-1.)*RoomHalfLength;
     PassageLamp(Center+FVector(X,Y,525),
       (Bay+Row)%2==0?FLinearColor(1.f,.56f,.31f):Definition.Accent);
     if(Row==1) continue;
     const FVector Pillar=Center+FVector(X,Y,CeilingHeight*.5);
     Block(Pillar,FVector(148,148,CeilingHeight),Stone);
     Block(Pillar-FVector(0,0,CeilingHeight*.5-26),FVector(210,210,52),Mark);
    }
   }
   const auto Fixture=[&](float X,float Y,float W,float D,float H)
   {
    const double Height=FMath::Min(235.,double(H));
    Block(Center+FVector(X*LayoutScale,Y*LayoutScale,Height/2),FVector(W*1.65,D*1.65,Height),Mark);
   };
   switch(Definition.Layout)
   {
    case ELayout::Pillars:
     for(float X : {-420.f,35.f,470.f}) for(float Y : {-385.f,385.f}) Fixture(X,Y,115,115,310);
     break;
    case ELayout::Web:
     Fixture(-155,-490,580,96,216);
     Fixture(350,470,615,105,240);
     Fixture(450,-590,195,205,170);
     break;
    case ELayout::Forge:
     Fixture(-135,5,220,470,235);
     Fixture(535,0,200,205,125);
     Fixture(670,-690,860,110,85);
     break;
    case ELayout::Marsh:
     // Raised stepping blocks leave the stone floor as the only continuous surface.
     for(int32 P=0;P<5;++P) Fixture(-470+315*P,P%2?260:-260,135,110,21);
     Fixture(90,420,210,120,155);
     Fixture(400,-475,240,170,180);
     break;
    case ELayout::Spires:
     for(int32 N=0;N<7;++N) Fixture(-430+220*N,N%2?435:-435,76,76,160+45*N);
     break;
    case ELayout::Maze:
     Fixture(-270,-335,75,870,290);
     Fixture(250,350,85,880,290);
     Fixture(820,-330,75,870,290);
     break;
    case ELayout::Ice:
     Fixture(-240,340,270,430,260);
     Fixture(240,-410,250,480,286);
     Fixture(790,420,220,340,235);
     break;
    case ELayout::Caldera:
     Fixture(120,25,250,490,60);
     Fixture(420,-650,920,110,100);
     Fixture(670,620,530,120,165);
     break;
    case ELayout::Cross:
     Fixture(120,0,510,90,265);
     Fixture(120,0,90,560,265);
     Fixture(680,565,125,130,295);
     break;
    case ELayout::Vault:
     for(float Y : {-650.f,650.f}) Fixture(290,Y,1120,100,145);
     Fixture(1100,0,252,410,88);
     break;
   }
   const FVector Encounter[]={{-490,220,95},{330,-200,95},{700,205,95},{1040,-175,95}};
   for(int32 Slot=0;Slot<UE_ARRAY_COUNT(Encounter);++Slot)
   {
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    if(auto* Enemy=GetWorld()->SpawnActor<ACryptMonster>(Center+FVector(Encounter[Slot].X*LayoutScale,Encounter[Slot].Y*LayoutScale,Encounter[Slot].Z),FRotator::ZeroRotator,Spawn))
    {
     Enemy->SaveId=EnemyId(I,Slot);
     Enemy->ConfigureCreature(Definition.Enemies[Slot],I);
    }
   }
  }
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
   Raider->ConfigureCreature(0);
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
  {
   Guardian->SaveId=100+i;
   Guardian->ConfigureCreature(1);
  }
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
