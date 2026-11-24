#include "CryptGameMode.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "CryptHudLayout.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "GameFramework/TouchInterface.h"
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
#include "Components/StaticMeshComponent.h"
#include "Engine/PointLight.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "Components/PointLightComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Engine.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
namespace
{
 constexpr float CastleApronCenterX=4450.f;
 constexpr float CastleApronLength=1650.f;
 constexpr float CastleApronStart=CryptWorld::CastleOffsetX+CastleApronCenterX-CastleApronLength*.5f;
 constexpr float CastleApronEnd=CastleApronStart+CastleApronLength;
 constexpr float CastleApronHalfWidth=1225.f;
 constexpr float MainRoadStart=-5500.f;
 constexpr float MainRoadHalfWidth=200.f;

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
  // Leave the village and its northern exit open through the old inner rim.
  const float TownOpening=(1.f-FMath::SmoothStep(1900.f,3500.f,
    FMath::Abs(X-static_cast<float>(CryptWorld::Town.X))))*FMath::SmoothStep(2500.f,4300.f,Y);
  Height+=760.f*FMath::SmoothStep(.96f,1.16f,Edge)*
   (1.f-FMath::SmoothStep(6300.f,12200.f,CoreDistance))*(1.f-TownOpening);
  const float Wilder=FMath::SmoothStep(6600.f,12200.f,CoreDistance);
  const float Ridges=610.f*FMath::Sin((X+.32f*Y)/5900.f)+420.f*FMath::Cos((Y-.24f*X)/4600.f);
  const float Rolling=175.f*FMath::Sin(X/1960.f)*FMath::Cos(Y/2420.f);
  Height+=Wilder*(Ridges+Rolling+750.f);
  const float MapEdge=FMath::Max(FMath::Abs(X)/CryptWorld::HalfWorldX,FMath::Abs(Y)/CryptWorld::HalfWorldY);
  if(MapEdge>.86f)
  {
   Height=FMath::Lerp(Height,CryptWorld::SeaLevel-1350.f,FMath::SmoothStep(.86f,1.01f,MapEdge));
   // Keep the coastal approach shallow, then fall to a 20-hero-height ocean floor.
   constexpr float DeepOceanDepth=20.f*192.f;
   Height-= (DeepOceanDepth-1350.f)*FMath::SmoothStep(1.01f,1.15f,MapEdge);
   for(const auto& Island : CryptWorld::Islands)
   {
    const double Distance=FVector2D::Distance(FVector2D(X,Y),Island.Center)/Island.Radius;
    const float Profile=static_cast<float>(1.-FMath::SmoothStep(.32,1.03,Distance));
    if(Profile>0.f)
    {
     const float Hills=210.f*FMath::Sin(X/1170.f)*FMath::Cos(Y/1600.f);
     const float IslandHeight=CryptWorld::SeaLevel-450.f+Profile*(1260.f+Hills);
     const float Shelf=static_cast<float>(1.-FMath::SmoothStep(.79,1.03,Distance));
     Height=FMath::Max(Height,FMath::Lerp(Height,IslandHeight,Shelf));
    }
   }
  }
  if(X<-6600.f)
  {
   const float ByTown=(1.f-FMath::SmoothStep(24900.f,27600.f,-X))*
      (1.f-FMath::SmoothStep(5850.f,8400.f,FMath::Abs(Y-CryptSettlements::DistrictNorth)));
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
  const float DistrictRoad=(1.f-FMath::SmoothStep(280.f,1350.f,FMath::Abs(X+6400.f)))
   *FMath::SmoothStep(-500.f,0.f,Y)
   *(1.f-FMath::SmoothStep(CryptSettlements::DistrictNorth,CryptSettlements::DistrictNorth+900.f,Y));
  const float EastRoad=FMath::SmoothStep(4600.f,5000.f,X)
   *(1.f-FMath::SmoothStep(CryptWorld::MainRoadEnd,CryptWorld::MainRoadEnd+600.f,X))
   *(1.f-FMath::SmoothStep(245.f,3250.f,FMath::Abs(Y)));
  const float CastleGround=(1.f-FMath::SmoothStep(840.f,1450.f,FMath::Abs(X-static_cast<float>(CryptWorld::Castle.X))))
   *(1.f-FMath::SmoothStep(1270.f,2050.f,FMath::Abs(Y)));
  const float TownGround=(1.f-FMath::SmoothStep(1750.f,3200.f,FMath::Abs(X-static_cast<float>(CryptWorld::Town.X))))
   *(1.f-FMath::SmoothStep(1680.f,3500.f,FMath::Abs(Y-CryptWorld::TownNorth)));
  const float TownRoad=(1.f-FMath::SmoothStep(325.f,1100.f,FMath::Abs(X-CryptWorld::TownRoadX)))
   *(1.f-FMath::SmoothStep(CryptWorld::TownNorth,CryptWorld::TownNorth+550.f,Y))
   *FMath::SmoothStep(-380.f,0.f,Y);
  const float RuinGround=(1.f-FMath::SmoothStep(1900.f,2400.f,FMath::Abs(X-(1000.f+CryptWorld::RuinOffsetX))))
   *(1.f-FMath::SmoothStep(1580.f,2170.f,FMath::Abs(Y-3200.f)));
  Height=FMath::Lerp(Height,-340.f,static_cast<float>(1.-FMath::SmoothStep(.54,1.08,CryptWorld::PoolRadius(X,Y))));
  return FMath::Lerp(Height,-20.f,FMath::Max3(FMath::Max3(FMath::Max(WestRoad,DistrictRoad),EastRoad,CastleGround),
   FMath::Max(TownGround,TownRoad),RuinGround));
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

 bool IsTrail(float X,float Y)
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
  return false;
 }

 bool IsDirt(float X,float Y)
 {
  if(IsTrail(X,Y)) return true;
  for(const auto& River : CryptWorld::Rivers)
   if(Y>River.Start && Y<River.End && FMath::Abs(X-River.X(Y))<River.BedWidth()+60.f)
    return true;
  return false;
 }

 void DisableTemplateGround(UWorld* World)
 {
  // All playable ground is created below. Keep startup lighting and sky,
  // but discard authored ground regardless of its material, size, or height.
  for(TActorIterator<AActor> It(World);It;++It)
  {
   AActor* Actor=*It;
   const bool bLandscape=Actor->GetClass()->GetName().StartsWith(TEXT("Landscape"));
   TArray<UPrimitiveComponent*> Primitives;
   Actor->GetComponents(Primitives);
   for(UPrimitiveComponent* Primitive : Primitives)
   {
    if(!bLandscape && !Primitive->IsA<UStaticMeshComponent>() &&
       !Primitive->IsA<UProceduralMeshComponent>() &&
       !Primitive->GetClass()->GetName().StartsWith(TEXT("Landscape"))) continue;

    Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // Sky domes may be static meshes. Keep their rendering only.
    if(!Actor->GetName().Contains(TEXT("Sky")) &&
       !Primitive->GetName().Contains(TEXT("Sky")))
     Primitive->SetHiddenInGame(true);
   }
  }
 }

 void CreateTerrain(UWorld* World,UMaterialInterface* Grass,UMaterialInterface* Dirt,UMaterialInterface* Overworld,
                    UMaterialInterface* Paving)
 {
  TArray<float> Eastings,Northings;
  for(float X=-CryptWorld::OceanLimit;X<=CryptWorld::OceanLimit;X+=1500.f) Eastings.Add(X);
  for(float Y=-CryptWorld::OceanLimit;Y<=CryptWorld::OceanLimit;Y+=1500.f) Northings.Add(Y);
  for(float X=-6600;X<=CryptWorld::East;X+=100) Eastings.AddUnique(X);
  for(float Y=-6000;Y<=CryptWorld::TownNorth+3600.f;Y+=100) Northings.AddUnique(Y);
  Eastings.AddUnique(-24900);
  Northings.AddUnique(CryptSettlements::DistrictNorth-5850.f);
  Northings.AddUnique(CryptSettlements::DistrictNorth+5850.f);
  for(const auto& P : CryptDungeons::EntrancePoints)
  {
   for(float X=P.X-300;X<=P.X+300;X+=100) Eastings.AddUnique(X);
   Eastings.AddUnique(P.X+CryptDungeons::RampLength);
   for(float Y=P.Y-300;Y<=P.Y+300;Y+=100) Northings.AddUnique(Y);
  }
  Eastings.AddUnique(MainRoadStart);
  Eastings.AddUnique(CastleApronStart);
  Eastings.AddUnique(CastleApronEnd);
  for(const float Side : {-1.f,1.f})
  {
   Northings.AddUnique(Side*MainRoadHalfWidth);
   Northings.AddUnique(Side*CastleApronHalfWidth);
  }
  // Sample the island crests and shorelines independently of the ocean grid.
  for(const auto& Island : CryptWorld::Islands)
  {
   for(int32 Sample=-6;Sample<=6;++Sample)
   {
    const float Offset=Island.Radius*Sample/5.f;
    Eastings.AddUnique(static_cast<float>(Island.Center.X)+Offset);
    Northings.AddUnique(static_cast<float>(Island.Center.Y)+Offset);
   }
  }
  Eastings.Sort();
  Northings.Sort();
  const int32 Columns=Eastings.Num()-1;
  const int32 Rows=Northings.Num()-1;
  const int32 Stride=Columns+1;
  // Use local indices for each strip so mobile rendering never needs a large index range.
  constexpr int32 RowsPerStrip=32;
  for(int32 FirstRow=0;FirstRow<Rows;FirstRow+=RowsPerStrip)
  {
   const int32 LastRow=FMath::Min(FirstRow+RowsPerStrip,Rows);
   auto* Surface=CreateSurface(World,TEXT("GroundSurface"),true);
   if(!Surface) continue;
   TArray<FVector> Vertices,Normals;
   TArray<FVector2D> UVs;
   TArray<FLinearColor> TerrainMasks;
   TArray<FProcMeshTangent> Tangents;
   TArray<int32> Triangles,DirtTriangles,PavingTriangles;
   Vertices.Reserve(Stride*(LastRow-FirstRow+1));
   Normals.Reserve(Stride*(LastRow-FirstRow+1));
   UVs.Reserve(Stride*(LastRow-FirstRow+1));
   TerrainMasks.Reserve(Stride*(LastRow-FirstRow+1));
   Tangents.Reserve(Stride*(LastRow-FirstRow+1));
   Triangles.Reserve(Columns*(LastRow-FirstRow)*6);
   DirtTriangles.Reserve(Columns*(LastRow-FirstRow));
   for(int32 Row=FirstRow;Row<=LastRow;++Row)
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
     const float WoodlandPattern=FMath::Sin(X/2850.f+Y/5200.f)*FMath::Cos(Y/3400.f-X/6900.f);
     const float Woodland=FMath::SmoothStep(-.42f,.55f,WoodlandPattern);
     const float Slope=FMath::Sqrt(Dx*Dx+Dy*Dy);
     const float Rocky=FMath::SmoothStep(.22f,.60f,Slope);
     float Bank=0.f;
     for(const auto& River : CryptWorld::Rivers)
     {
      if(Y<=River.Start || Y>=River.End) continue;
      const float Distance=FMath::Abs(X-River.X(Y));
      const float Edge=River.BedWidth();
      const float Fade=FMath::SmoothStep(0.f,250.f,FMath::Min(Y-River.Start,River.End-Y));
      Bank=FMath::Max(Bank,Fade*(1.f-FMath::SmoothStep(Edge+10.f,Edge+260.f,Distance)));
     }
     TerrainMasks.Emplace(Woodland,Rocky,Bank,IsTrail(X,Y)?1.f:0.f);
     if(Row<LastRow && Column<Columns)
     {
      const float TileX=(X+Eastings[Column+1])*.5f;
      const float TileY=(Y+Northings[Row+1])*.5f;
      if((TileX>-24900 && TileX<-6600 && FMath::Abs(TileY-CryptSettlements::DistrictNorth)<5850) ||
         CryptDungeons::IsRampCell(TileX,TileY)) continue;
      const int32 A=(Row-FirstRow)*Stride+Column;
      const int32 B=A+1;
      const int32 C=A+Stride;
      const int32 D=C+1;
      const bool bRoad=TileX>=MainRoadStart && TileX<CastleApronStart && FMath::Abs(TileY)<MainRoadHalfWidth;
      const bool bApron=TileX>=CastleApronStart && TileX<CastleApronEnd && FMath::Abs(TileY)<CastleApronHalfWidth;
      auto& Section=Paving && (bRoad || bApron)?PavingTriangles:
        (!Overworld && Dirt && IsDirt(TileX,TileY))?DirtTriangles:Triangles;
      Section.Append({A,C,B,B,C,D});
     }
    }
   }
   Surface->CreateMeshSection_LinearColor(0,Vertices,Triangles,Normals,UVs,TerrainMasks,Tangents,true);
   if(Overworld || Grass) Surface->SetMaterial(0,Overworld?Overworld:Grass);
   if(!DirtTriangles.IsEmpty())
   {
    Surface->CreateMeshSection_LinearColor(1,Vertices,DirtTriangles,Normals,UVs,{},Tangents,true);
    Surface->SetMaterial(1,Dirt);
   }
   if(!PavingTriangles.IsEmpty())
   {
    Surface->CreateMeshSection_LinearColor(2,Vertices,PavingTriangles,Normals,UVs,{},Tangents,true);
    Surface->SetMaterial(2,Paving);
   }
  }
 }


 void PlantOverworld(UWorld* World)
 {
  auto* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
  auto* Cone=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cone.Cone"));
  auto* Crown=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
  auto* Basic=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
  if(!Cylinder || !Cone || !Crown || !Basic) return;
  auto* Grove=World->SpawnActor<AActor>();
  if(!Grove) return;
  auto* Root=NewObject<USceneComponent>(Grove);
  Grove->SetRootComponent(Root);
  Grove->AddInstanceComponent(Root);
  Root->RegisterComponent();
  const auto Batch=[&](UStaticMesh* Model,FLinearColor Shade,bool bSolid)
  {
   auto* Parts=NewObject<UHierarchicalInstancedStaticMeshComponent>(Grove);
   auto* Tint=UMaterialInstanceDynamic::Create(Basic,Grove);
   Tint->SetVectorParameterValue(TEXT("Color"),Shade);
   Grove->AddInstanceComponent(Parts);
   Parts->SetupAttachment(Root);
   Parts->SetMobility(EComponentMobility::Static);
   Parts->SetStaticMesh(Model);
   Parts->SetMaterial(0,Tint);
   Parts->SetCollisionProfileName(bSolid?TEXT("BlockAll"):TEXT("NoCollision"));
   Parts->SetCanEverAffectNavigation(false);
   Parts->SetCullDistances(9600,12000);
   Parts->SetCastShadow(bSolid);
   Parts->RegisterComponent();
   return Parts;
  };
  auto* Trunks=Batch(Cylinder,FLinearColor(.18f,.086f,.042f),true);
  auto* Boughs=Batch(Cylinder,FLinearColor(.25f,.126f,.057f),false);
  auto* PineDark=Batch(Cone,FLinearColor(.052f,.15f,.086f),false);
  auto* PineLight=Batch(Cone,FLinearColor(.087f,.224f,.105f),false);
  auto* OakDark=Batch(Crown,FLinearColor(.10f,.235f,.069f),false);
  auto* OakLight=Batch(Crown,FLinearColor(.21f,.343f,.096f),false);
  auto* Autumn=Batch(Crown,FLinearColor(.40f,.271f,.067f),false);
  const auto Add=[](auto* Parts,FVector Position,FVector Size,FRotator Rotation=FRotator::ZeroRotator)
  {
   Parts->AddInstance(FTransform(Rotation,Position,Size/100.));
  };
  const auto Branch=[&](auto* Parts,FVector Bottom,FVector Top,double Width)
  {
   const FVector Span=Top-Bottom;
   Add(Parts,(Bottom+Top)*.5,FVector(Width,Width,Span.Size()),FRotationMatrix::MakeFromZ(Span).Rotator());
  };
  FRandomStream Random(142887);
  const auto Plant=[&](float X,float Y)
  {
   if((X>-6800.f && X<CryptWorld::MainRoadEnd+400.f && FMath::Abs(Y)<590.f) ||
      (X>-25200.f && X<-6300.f && FMath::Abs(Y-CryptSettlements::DistrictNorth)<6200.f) ||
      (X>510.f && X<4280.f && Y>CryptWorld::TownNorth-1770.f && Y<CryptWorld::TownNorth+1700.f) ||
      (FMath::Abs(X-CryptWorld::TownRoadX)<550.f && Y>0 && Y<CryptWorld::TownNorth) ||
      (FMath::Abs(X-CryptWorld::Castle.X)<1460. && FMath::Abs(Y)<1750.f) ||
      (FMath::Abs(X-(1000.f+CryptWorld::RuinOffsetX))<2000.f && FMath::Abs(Y-3200.f)<1660.f) ||
      CryptWorld::PoolRadius(X,Y)<1.11 || IsDirt(X,Y)) return;
   for(const auto& Mouth : CryptDungeons::EntrancePoints)
    if(FVector2D::DistSquared(Mouth,FVector2D(X,Y))<FMath::Square(1700.)) return;
   FHitResult Ground;
   if(!World->LineTraceSingleByChannel(Ground,FVector(X,Y,3500),FVector(X,Y,-1800),ECC_Visibility) ||
      Ground.ImpactNormal.Z<.73 || Ground.ImpactPoint.Z<-490.) return;
   const FVector Foot=Ground.ImpactPoint-FVector(0,0,24);
   const float Height=Random.FRandRange(385,850);
   const float Stem=Height*Random.FRandRange(.046f,.070f);
   const FVector Lean(Random.FRandRange(-35,35),Random.FRandRange(-35,35),0);
   const bool bPine=Random.FRand()<.53f;
   Branch(Trunks,Foot,Foot+FVector(0,0,Height*.44),Stem);
   Branch(Boughs,Foot+FVector(0,0,Height*.37),Foot+Lean+FVector(0,0,Height*.86),Stem*.67);
   for(int32 Limb=0;Limb<5;++Limb)
   {
    const double Angle=Limb*2.399963+Random.FRandRange(-.20f,.20f);
    const FVector Direction(FMath::Cos(Angle),FMath::Sin(Angle),0);
    const float Level=Height*(.43f+Limb*.071f);
    const float Spread=Height*(bPine?.24f-Limb*.031f:.18f);
    const FVector Fork=Foot+Lean*.6+FVector(0,0,Level);
    const FVector Tip=Fork+Direction*Spread+FVector(0,0,Height*.15);
    Branch(Boughs,Fork,Tip,Stem*.30);
    if(bPine)
    {
     const FVector Needles=Fork+Direction*Spread*.44+FVector(0,0,Height*.099);
     const float Width=Height*(.53f-Limb*.073f);
     Add(Limb%2?PineLight:PineDark,Needles,FVector(Width,Width*.87,Height*.31),FRotator(0,Limb*37,0));
    }
    else
    {
     const float Width=Height*Random.FRandRange(.24f,.36f);
     Add(Limb%2?OakLight:OakDark,Tip,FVector(Width,Width*.91,Width*.73),FRotator(0,Limb*44,0));
    }
   }
   if(bPine)
    Add(PineLight,Foot+Lean+FVector(0,0,Height*.88),FVector(Height*.22,Height*.19,Height*.30));
   else
    Add(Random.FRand()<.16f?Autumn:OakLight,Foot+Lean+FVector(0,0,Height*.87),
       FVector(Height*.40,Height*.35,Height*.29));
  };
  for(float X=-63000.f;X<=63000.f;X+=3100.f)
   for(float Y=-57000.f;Y<=57000.f;Y+=3100.f)
    if(Random.FRand()<.78f)
     Plant(X+Random.FRandRange(-990,990),Y+Random.FRandRange(-990,990));
  for(const auto& Island : CryptWorld::Islands)
   for(int32 Tree=0;Tree<19;++Tree)
   {
    const double Angle=Tree*2.39996;
    const double Radius=Island.Radius*(.14+.45*FMath::Sqrt(Tree/19.));
    const float X=Island.Center.X+Radius*FMath::Cos(Angle);
    const float Y=Island.Center.Y+Radius*FMath::Sin(Angle);
    if(TerrainHeight(X,Y)>CryptWorld::SeaLevel+100.f) Plant(X,Y);
   }
  for(float X=-5900.f;X<=12600.f;X+=930.f)
   for(float Y=-6000.f;Y<=6200.f;Y+=950.f)
    if(Random.FRand()<.78f)
     Plant(X+Random.FRandRange(-240,240),Y+Random.FRandRange(-240,240));
 }

 void CreateGravehollowPool(UWorld* World,UMaterialInterface* Water)
 {
  using namespace CryptWorld;
  if(!Water) return;
  auto* Surface=CreateSurface(World,TEXT("GravehollowPool"),false);
  if(!Surface) return;
  constexpr int32 Segments=64;
  const FVector Center(PoolX,PoolY,PoolSurface);
  TArray<FVector> Vertices{Center};
  TArray<FVector2D> UVs{FVector2D(.5,.5)};
  TArray<int32> Indices;
  for(int32 Index=0;Index<Segments;++Index)
  {
   const double Angle=2.*PI*Index/Segments;
   const double X=FMath::Cos(Angle),Y=FMath::Sin(Angle);
   Vertices.Add(Center+FVector(X*PoolRadiusX,Y*PoolRadiusY,0));
   UVs.Emplace(.5+X*.5,.5+Y*.5);
   Indices.Append({0,1+(Index+1)%Segments,1+Index});
  }
  Surface->CreateMeshSection_LinearColor(0,Vertices,Indices,{},UVs,{}, {},false);
  Surface->SetMaterial(0,Water);
  Surface->SetCastShadow(false);

  auto* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
  auto* Cone=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cone.Cone"));
  auto* Basic=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
  if(!Sphere || !Cone || !Basic) return;
  const auto Part=[&](AActor* Owner,UStaticMesh* Mesh,FLinearColor Color,
                       FVector Position,FVector Scale,FRotator Rotation=FRotator::ZeroRotator)
  {
   auto* Component=NewObject<UStaticMeshComponent>(Owner);
   Owner->AddInstanceComponent(Component);
   Component->SetupAttachment(Owner->GetRootComponent());
   Component->SetStaticMesh(Mesh);
   auto* Tint=UMaterialInstanceDynamic::Create(Basic,Component);
   Tint->SetVectorParameterValue(TEXT("Color"),Color);
   Component->SetMaterial(0,Tint);
   Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
   Component->SetCastShadow(false);
   Component->SetRelativeTransform(FTransform(Rotation,Position,Scale));
   Component->RegisterComponent();
  };
  TArray<TWeakObjectPtr<AStaticMeshActor>> Fish;
  const FLinearColor FishColors[]={{.16f,.55f,.58f},{.83f,.46f,.17f},{.52f,.69f,.78f}};
  for(int32 Index=0;Index<14;++Index)
  {
   auto* Actor=World->SpawnActor<AStaticMeshActor>(Center-FVector(0,0,65),FRotator::ZeroRotator);
   if(!Actor) continue;
   Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
   const FLinearColor Color=FishColors[Index%3];
   Part(Actor,Sphere,Color,FVector::ZeroVector,FVector(.34,.13,.18));
   Part(Actor,Cone,Color,FVector(-20,0,0),FVector(.17,.26,.18),FRotator(90,0,0));
   Part(Actor,Cone,Color,FVector(-2,0,8),FVector(.10,.12,.15));
   Fish.Add(Actor);
  }
  for(int32 Index=0;Index<9;++Index)
  {
   const double Angle=Index*2.4;
   FVector Base=Center+FVector(FMath::Cos(Angle)*420,FMath::Sin(Angle)*310,0);
   Base.Z=TerrainHeight(Base.X,Base.Y)+9.;
   for(int32 Arm=0;Arm<5;++Arm)
   {
    const double Yaw=72.*Arm;
    const FRotator Rotation(0,Yaw,0);
    auto* Star=World->SpawnActor<AStaticMeshActor>(Base+Rotation.Vector()*9.,Rotation);
    if(!Star) continue;
    Star->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
    Part(Star,Sphere,FLinearColor(.86,.26,.12),FVector::ZeroVector,FVector(.26,.10,.065));
   }
   auto* Plant=World->SpawnActor<AStaticMeshActor>(Base+FVector(22,12,0),FRotator::ZeroRotator);
   if(!Plant) continue;
   Plant->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
   for(int32 Leaf=-1;Leaf<=1;++Leaf)
    Part(Plant,Sphere,FLinearColor(.09,.34,.13),FVector(Leaf*9,0,17),
      FVector(.09,.045,.38),FRotator(0,0,Leaf*24));
  }
  FTimerHandle SwimTimer;
  World->GetTimerManager().SetTimer(SwimTimer,FTimerDelegate::CreateWeakLambda(Surface,
   [Fish=MoveTemp(Fish),Center,Elapsed=0.]() mutable
   {
    Elapsed+=.05;
    for(int32 Index=0;Index<Fish.Num();++Index)
    {
     auto* Actor=Fish[Index].Get();
     if(!Actor) continue;
     const double Direction=Index%2==0?1.:-1.;
     const double Phase=Index*2.39996+Direction*Elapsed*(.27+.022*(Index%4));
     const double Radius=310.+45.*(Index%5);
     const FVector Offset(FMath::Cos(Phase)*Radius,FMath::Sin(Phase)*Radius*.74,
      -55.-18.*(Index%5)+12.*FMath::Sin(Phase*2.));
     const FVector Forward=Direction*FVector(-FMath::Sin(Phase),FMath::Cos(Phase)*.74,0);
     Actor->SetActorLocationAndRotation(Center+Offset,Forward.Rotation(),false);
    }
   }),.05f,true);
 }

 void CreateOceanFleet(UWorld* World,UMaterialInterface* Water)
 {
  using namespace CryptWorld;
  if(Water)
  {
   if(auto* Surface=CreateSurface(World,TEXT("OceanSurface"),false))
   {
    const double L=OceanLimit;
    Surface->CreateMeshSection_LinearColor(0,
     {FVector(-L,-L,SeaLevel),FVector(L,-L,SeaLevel),FVector(-L,L,SeaLevel),FVector(L,L,SeaLevel)},
     {0,2,1,1,2,3},{FVector::UpVector,FVector::UpVector,FVector::UpVector,FVector::UpVector},
     {FVector2D(0,0),FVector2D(85,0),FVector2D(0,85),FVector2D(85,85)},{},{},false);
    Surface->SetMaterial(0,Water);
    Surface->SetCastShadow(false);
   }
  }
  auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
  auto* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
  auto* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
  auto* Basic=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
  if(!Cube || !Sphere || !Cylinder || !Basic) return;
  const auto Part=[&](AActor* Owner,UStaticMesh* Mesh,FVector At,FVector Size,FLinearColor Color,
                       FRotator Rotation=FRotator::ZeroRotator,bool bCollision=false)
  {
   auto* Component=NewObject<UStaticMeshComponent>(Owner);
   Owner->AddInstanceComponent(Component);
   Component->SetupAttachment(Owner->GetRootComponent());
   Component->SetStaticMesh(Mesh);
   auto* Tint=UMaterialInstanceDynamic::Create(Basic,Component);
   if(Tint)
   {
    Tint->SetVectorParameterValue(TEXT("Color"),Color);
    Component->SetMaterial(0,Tint);
   }
   Component->SetCollisionProfileName(bCollision?TEXT("BlockAll"):TEXT("NoCollision"));
   Component->SetRelativeTransform(FTransform(Rotation,At,Size/100.));
   Component->RegisterComponent();
  };
  const FLinearColor Wood(.21f,.10f,.045f), Planks(.42f,.24f,.12f), Canvas(.83f,.76f,.57f);
  for(int32 Index=0;Index<=UE_ARRAY_COUNT(Islands);++Index)
  {
   const FVector Out=DockDirection(Index);
   const double Length=Index==0?9000.:2800.;
   const FVector End=DockEnd(Index);
   const FVector Center=End-Out*Length*.5;
   auto* Dock=World->SpawnActor<AStaticMeshActor>(Center,Out.Rotation());
   if(!Dock) continue;
   Part(Dock,Cube,FVector::ZeroVector,FVector(Length,460,36),Planks,FRotator::ZeroRotator,true);
   for(double X=-Length*.5+200.;X<Length*.5;X+=480.)
    for(const double Y : {-195.,195.})
     Part(Dock,Cylinder,FVector(X,Y,-130),FVector(42,42,480),Wood);
  }
  TArray<TWeakObjectPtr<AStaticMeshActor>> Patrols;
  for(int32 Index=0;Index<4;++Index)
  {
   // Two ships start moored; the other two sail offshore until boarded.
   const bool bPatrol=Index>=2;
   const FVector Start=bPatrol?FVector(78000.-(Index-2)*155000.,-32000,SeaLevel):ShipBerth(Index);
   auto* Ship=World->SpawnActor<AStaticMeshActor>(Start,bPatrol?FRotator::ZeroRotator:DockDirection(Index).Rotation());
   if(!Ship) continue;
   Ship->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
   Ship->Tags.Add(TEXT("OceanShip"));
   Part(Ship,Sphere,FVector(0,0,-20),FVector(1900,610,430),Wood);
   Part(Ship,Cube,FVector(0,0,120),FVector(1530,480,38),Planks,FRotator::ZeroRotator,true);
   for(const double Side : {-1.,1.})
    Part(Ship,Cube,FVector(0,Side*244,167),FVector(1500,24,75),Wood);
   for(const double X : {-460.,0.,450.})
   {
    Part(Ship,Cylinder,FVector(X,0,575),FVector(52,52,930),Wood);
    Part(Ship,Cylinder,FVector(X,0,790),FVector(29,29,680),Wood,FRotator(0,0,90));
    Part(Ship,Sphere,FVector(X+36,0,620),FVector(135,620,345),Canvas);
   }
   Part(Ship,Cube,FVector(-460,98,979),FVector(12,195,90),FLinearColor(.05f,.05f,.06f));
   if(bPatrol) Patrols.Add(Ship);
  }
  FTimerHandle PatrolTimer;
  World->GetTimerManager().SetTimer(PatrolTimer,FTimerDelegate::CreateWeakLambda(World,
   [Patrols=MoveTemp(Patrols),Time=0.]() mutable
   {
    Time+=.08;
    for(int32 Index=0;Index<Patrols.Num();++Index)
    {
     auto* Ship=Patrols[Index].Get();
     if(!Ship || Ship->ActorHasTag(TEXT("ClaimedShip"))) continue;
     const double Angle=Time*.052+Index*PI;
     const FVector Position((Index==0?81000.:-81000.)+FMath::Cos(Angle)*5200.,
      -36000.+FMath::Sin(Angle)*13000.,SeaLevel+FMath::Sin(Time*1.6)*12.);
     const FVector Heading(-FMath::Sin(Angle)*5200.,FMath::Cos(Angle)*13000.,0.);
     Ship->SetActorLocationAndRotation(Position,Heading.Rotation());
    }
   }),.08f,true);
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
   TArray<FLinearColor> WaterMasks;
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
     const float Splash=River.bFalls?
      FMath::SmoothStep(.33f,.44f,T)*(1.f-FMath::SmoothStep(.46f,.50f,T)):0.f;
     WaterMasks.Emplace(Splash,0.f,0.f,1.f);
     Tangents.Emplace(FVector::ForwardVector,false);
    }
    if(Index<Samples)
    {
     auto& Section=(River.bFalls && T>=.46f && T<=.54f)?Falls:Stream;
     const int32 A=Index*2;
     Section.Append({A,A+2,A+1,A+1,A+2,A+3});
    }
   }
   Surface->CreateMeshSection_LinearColor(0,Vertices,Stream,Normals,UVs,WaterMasks,Tangents,false);
   Surface->SetMaterial(0,RiverMaterial);
   if(!Falls.IsEmpty())
   {
    Surface->CreateMeshSection_LinearColor(1,Vertices,Falls,Normals,UVs,WaterMasks,Tangents,false);
    Surface->SetMaterial(1,FallsMaterial);
   }
  }
 }
}

double CryptDungeons::GetGroundHeight(double X,double Y)
{
 return TerrainHeight(X,Y);
}

namespace
{
 class SCryptVirtualJoystick : public SVirtualJoystick
 {
 public:
  TWeakObjectPtr<ACryptPlayerController> Owner;

  FReply OnTouchStarted(const FGeometry& Geometry,const FPointerEvent& Event) override
  {
   if(auto* Hero=GetHero();Hero && !Hero->bInventoryOpen)
   {
    FVector2D Point,Size;
    if(ViewportPoint(Geometry,Event,Point,Size))
    {
     const FCryptHudLayout Layout(Size.X,Size.Y);
     if(FVector2D::Distance(Point,Layout.RightThumbCenter)<=Layout.RightThumbRadius)
      Hero->TouchPressed(Finger(Event),FVector(Point.X,Point.Y,0));
    }
   }
   return SVirtualJoystick::OnTouchStarted(Geometry,Event);
  }

  FReply OnTouchMoved(const FGeometry& Geometry,const FPointerEvent& Event) override
  {
   FVector2D Point,Size;
   if(auto* Hero=GetHero();Hero && ViewportPoint(Geometry,Event,Point,Size))
    Hero->TouchMoved(Finger(Event),FVector(Point.X,Point.Y,0));
   return SVirtualJoystick::OnTouchMoved(Geometry,Event);
  }

  FReply OnTouchEnded(const FGeometry& Geometry,const FPointerEvent& Event) override
  {
   FVector2D Point,Size;
   if(auto* Hero=GetHero();Hero && ViewportPoint(Geometry,Event,Point,Size))
    Hero->TouchReleased(Finger(Event),FVector(Point.X,Point.Y,0));
   return SVirtualJoystick::OnTouchEnded(Geometry,Event);
  }

 private:
  ACryptHero* GetHero() const { return Owner.IsValid()?Cast<ACryptHero>(Owner->GetPawn()):nullptr; }

  static ETouchIndex::Type Finger(const FPointerEvent& Event)
  {
   return static_cast<ETouchIndex::Type>(Event.GetPointerIndex());
  }

  static bool ViewportPoint(const FGeometry& Geometry,const FPointerEvent& Event,FVector2D& Point,FVector2D& Size)
  {
   if(!GEngine || !GEngine->GameViewport) return false;
   GEngine->GameViewport->GetViewportSize(Size);
   const FVector2D LocalSize=Geometry.GetLocalSize();
   if(Size.X<=0 || Size.Y<=0 || LocalSize.X<=0 || LocalSize.Y<=0) return false;
   const FVector2D Local=Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
   Point=FVector2D(Local.X*Size.X/LocalSize.X,Local.Y*Size.Y/LocalSize.Y);
   return true;
  }
 };
}

void ACryptPlayerController::CreateTouchInterface()
{
 Super::CreateTouchInterface();
 if(!IsLocalController()) return;
 auto* Default=LoadObject<UTouchInterface>(nullptr,
  TEXT("/Engine/MobileResources/HUD/DefaultVirtualJoysticks.DefaultVirtualJoysticks"));
 if(!Default || Default->Controls.Num()<2) return;
 auto* Fixed=DuplicateObject<UTouchInterface>(Default,this);
 Fixed->bPreventRecenter=true;
 Fixed->Controls[0].Center=FCryptHudLayout::LeftStickAnchor();
 Fixed->Controls[1].Center=FCryptHudLayout::RightStickAnchor();
 ActivateTouchInterface(Fixed);
}

TSharedPtr<SVirtualJoystick> ACryptPlayerController::CreateVirtualJoystick()
{
 TSharedRef<SCryptVirtualJoystick> Joystick=SNew(SCryptVirtualJoystick);
 Joystick->Owner=this;
 return Joystick;
}

ACryptGameMode::ACryptGameMode() { PlayerControllerClass=ACryptPlayerController::StaticClass(); DefaultPawnClass = ACryptHero::StaticClass(); HUDClass = ACryptMobileHUD::StaticClass(); }
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
 // Supply the mobile renderer with a real IsSky surface enclosing the ocean.
 auto* SkyMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Environment/MindControl/M_MobileAtmosphereSky.M_MobileAtmosphereSky"));
 auto* SkySphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
 if(SkyMaterial && SkySphere)
 {
  auto* Dome=GetWorld()->SpawnActor<AStaticMeshActor>(FVector::ZeroVector,FRotator::ZeroRotator);
  if(Dome)
  {
   Dome->Tags.Add(TEXT("MobileAtmosphereSky"));
   auto* Mesh=Dome->GetStaticMeshComponent();
   Mesh->SetMobility(EComponentMobility::Movable);
   Mesh->SetStaticMesh(SkySphere);
   Mesh->SetMaterial(0,SkyMaterial);
   Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
   Mesh->SetCastShadow(false);
   Dome->SetActorScale3D(FVector(20000.f));
  }
 }
 else UE_LOG(LogTemp,Error,TEXT("Mobile atmosphere sky material or sphere missing"));
 bool bHasSun=false;
 for(TActorIterator<ADirectionalLight> It(GetWorld());It;++It)
 {
  auto* Sun=Cast<UDirectionalLightComponent>(It->GetLightComponent());
  if(!Sun) continue;
  It->SetActorRotation(FRotator(-48.f,-35.f,0.f));
  Sun->SetIntensity(4.f);
  bHasSun=true;
 }
 if(!bHasSun)
 {
  auto* SunActor=GetWorld()->SpawnActor<ADirectionalLight>(FVector::ZeroVector,FRotator(-48.f,-35.f,0.f));
  if(SunActor) SunActor->GetLightComponent()->SetIntensity(4.f);
 }
 for(TActorIterator<ASkyLight> It(GetWorld());It;++It)
 {
  if(auto* Sky=It->GetLightComponent()) Sky->SetIntensity(1.5f);
 }
 auto* Overworld=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Environment/MindControl/M_Overworld.M_Overworld"));
 CreateTerrain(GetWorld(),Grass,Dirt,Overworld,Cobble);
 CreateWater(GetWorld(),River,Falls);
 CreateGravehollowPool(GetWorld(),LoadObject<UMaterialInterface>(nullptr,
  TEXT("/Game/Environment/MindControl/M_GravehollowPool.M_GravehollowPool")));
 auto* SeaMaterial=LoadObject<UMaterialInterface>(nullptr,
  TEXT("/Game/Environment/MindControl/M_GravehollowPool.M_GravehollowPool"));
 CreateOceanFleet(GetWorld(),SeaMaterial?SeaMaterial:River);
 // Keep movement inside the finished terrain while the surrounding hills continue past the boundary.
 for(const float Side : {-1.f,1.f})
 {
  for(const bool bAlongX : {false,true})
  {
   auto* Boundary=GetWorld()->SpawnActor<AStaticMeshActor>(
     FVector(bAlongX?0.f:Side*(CryptWorld::OceanLimit-80),bAlongX?Side*(CryptWorld::OceanLimit-80):0.f,3000),FRotator::ZeroRotator);
   if(!Boundary) continue;
   auto* Collision=Boundary->GetStaticMeshComponent();
   Collision->SetMobility(EComponentMobility::Movable);
   Collision->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
   Collision->SetCollisionProfileName(TEXT("BlockAll"));
   Boundary->SetActorScale3D(bAlongX?FVector(2*CryptWorld::OceanLimit/100,1,130):FVector(1,2*CryptWorld::OceanLimit/100,130));
   Boundary->SetActorHiddenInGame(true);
   Collision->SetMobility(EComponentMobility::Static);
  }
 }
 UMaterialInterface* Stone=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Environment/MindControl/M_CastleStone.M_CastleStone"));
 UMaterialInterface* RoadMaterial=Cobble?Cobble:Stone?Stone:Dirt?Dirt:Grass;
 // Wilderness stones and ruined pillars beside the approach.
 for(int32 i=0;i<10;++i)
 {
  const float X=-3100+i*420.f;
  const float Y=(i%2==0?1.f:-1.f)*(600.f+(i%3)*220.f);
  PlaceBlock(FVector(X,Y,75),FVector(1.2f,1.2f,2.5f+(i%3)),FRotator::ZeroRotator,Stone);
 }
 PlaceBlock(FVector(-6050,0,-25),FVector(11,4,.10),FRotator::ZeroRotator,RoadMaterial);
 const float DistrictNorth=CryptSettlements::DistrictNorth;
 PlaceBlock(FVector(-6400,DistrictNorth*.5f,-12),FVector(4.f,DistrictNorth/100.f,.14f),
  FRotator::ZeroRotator,RoadMaterial);
 const FVector TownOffset(0,CryptWorld::TownNorth,0);
 const FVector RuinOffset(CryptWorld::RuinOffsetX,0,0);
 const float TownRoad=CryptWorld::TownRoadX;
 PlaceBlock(FVector(TownRoad,CryptWorld::TownNorth*.5f+100.f,-10),
  FVector(3.8,(CryptWorld::TownNorth-200.f)/100.,.16),FRotator::ZeroRotator,RoadMaterial);
 PlaceBlock(FVector(TownRoad-170.f,CryptWorld::TownNorth,-9),
  FVector(3.5,3.8,.17),FRotator::ZeroRotator,RoadMaterial);
 CryptSettlements::SpawnMerchant(GetWorld(),FVector(2400,435,25)+TownOffset,20);
 CryptSettlements::Build(GetWorld(),Grass,Stone,RoadMaterial,Dirt?Dirt:Stone,-20.f);
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
    // Use the sculpt as side masonry. The collision-built arch owns the opening.
    if(auto* RockFace=GetWorld()->SpawnActor<AStaticMeshActor>(Origin+FVector(35,640,0),FRotator(0,90,0)))
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
       Random.FRandRange(-18,18),Random.FRandRange(-12,12)),Stone,Boulder);
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
    const double OffsetY=Side*Random.FRandRange(370,785);
    const double Ground=TerrainHeight(Origin.X+OffsetX,Origin.Y+OffsetY);
    const float Size=Random.FRandRange(65,225);
    Rock(FVector(OffsetX,OffsetY,Ground-Origin.Z+Size*.26),FVector(Size*1.22,Size,Size*.75));
   }

   if(bSinkhole)
   {
    for(double Side : {-1.,1.})
     for(int32 R=0;R<5;++R)
      Rock(FVector(-140+R*80,Side*(260+R*15),28),FVector(138,128,109));

   }
   else if(bCutStone)
   {
    // Recessed lintel and stepped jambs on a textured escarpment.
    for(const double Side : {-1.,1.})
    {
     Rock(FVector(190,Side*365,151),FVector(670,364,439));
     Rock(FVector(230,Side*457,250),FVector(583,285,438));
     Block(Origin+FVector(-3,Side*197,121),FVector(126,61,255),Stone);
     if(!Sculpt) Block(Origin+FVector(-37,Side*237,109),FVector(104,35,229),Stone);
     Block(Origin+FVector(-17,Side*216,18),FVector(151,85,41),Stone);
    }
    Block(Origin+FVector(-3,0,263),FVector(160,354,52),Stone);
    Block(Origin+FVector(-18,0,297),FVector(164,421,29),Stone);
    Rock(FVector(190,0,438),FVector(673,752,229));
   }
   else
   {
    // Layered boulders form an irregular arch without filling its opening.
    for(const double Side : {-1.,1.})
    {
     Rock(FVector(131,Side*295,123),FVector(462,235,327));
     Rock(FVector(163,Side*346,157),FVector(469,295,360));
     Rock(FVector(209,Side*254,258),FVector(449,195,157));
    }
    Rock(FVector(138,0,387),FVector(448,490,169));
    Rock(FVector(362,0,450),FVector(444,565,166));
   }

   if(!bSinkhole)
   {
    for(int32 Step=0;Step<3;++Step)
    {
     const double X=Origin.X-193+Step*69;
     const double Ground=TerrainHeight(X,Origin.Y);
     Block(FVector(X,Origin.Y,Ground+1),FVector(62,220,20),RoadMaterial);
    }
   }

   for(const double Side : {-1.,1.})
   {
    if(bRunes)
    {
     // Original angular glyphs inset along each edge, visible from the trail.
     for(int32 Row=0;Row<4;++Row)
     {
      const FVector Glyph=Origin+FVector(-114,Side*202,77+Row*43);
      Block(Glyph,FVector(3,5,27),Rune);
      PlaceBlock(Glyph+FVector(0,Side*8,4),FVector(.03,.22,.035),FRotator(0,0,Side*36),Rune,Cube);
      PlaceBlock(Glyph+FVector(0,-Side*5,-7),FVector(.03,.18,.035),FRotator(0,0,-Side*43),Rune,Cube);
     }
    }
    else if(!bSinkhole)
    {
     const FVector Brazier=Origin+FVector(-82,Side*245,79);
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

   constexpr double PassageHeadroom=300.;
   constexpr double RoofThickness=56.;
   const FVector RoofCenter=(Mouth+End)*.5+FVector(0,0,PassageHeadroom)
      +Pitch.RotateVector(FVector(0,0,RoofThickness*.5));
   PlaceBlock(RoofCenter,FVector(Run.Size(),2*HalfWidth+48.,RoofThickness)/100.,Pitch,Stone,Cube);

   for(int32 Section=0;Section<10;++Section)
   {
    const double Fraction=(Section+.5)/10.;
    const double X=Mouth.X+RampLength*Fraction;
    const double Bottom=FMath::Lerp(Mouth.Z,Center.Z,(Section+1)/10.)-30;
    for(double Side : {-1.,1.})
    {
     const double Y=Mouth.Y+Side*(HalfWidth+10);
     const double RoofTop=FMath::Lerp(Mouth.Z,Center.Z,Section/10.)+PassageHeadroom+RoofThickness+8.;
     const double Top=FMath::Max(RoofTop,GetGroundHeight(X,Y)+40);
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
   const double LintelBottom=PassageHeadroom-15.;
   Block(FVector(East+6,Center.Y,Center.Z+(CeilingHeight+LintelBottom)*.5),
         FVector(22,2*HalfWidth+12,CeilingHeight-LintelBottom),Stone);
   Block(Center+FVector(RoomHalfLength,0,CeilingHeight*.5),FVector(32,2*RoomHalfWidth,CeilingHeight),Stone);
   for(double Fraction : {.20,.52,.84})
   {
    const FVector Point=FMath::Lerp(Mouth,End,Fraction);
    PassageLamp(Point+FVector(0,0,245),FLinearColor(1.f,.58f,.32f));
   }
   // Each carved passage joins two cells in a spanning tree. The boss arena
   // has one entrance, at the east-side cell furthest from the ramp by foot.
   constexpr int32 Columns=19, Rows=11, Cells=Columns*Rows;
   constexpr double WallThickness=62.;
   const double MazeEnd=Center.X+RoomHalfLength*.47;
   const double CellWidth=(MazeEnd-East)/Columns;
   const double CellDepth=2*RoomHalfWidth/Rows;
   const double SouthEdge=Center.Y-RoomHalfWidth;
   const auto CellPoint=[&](int32 Cell)
   {
    return FVector(East+(Cell%Columns+.5)*CellWidth,SouthEdge+(Cell/Columns+.5)*CellDepth,Center.Z);
   };

   int32 Parent[Cells], Depth[Cells]{};
   for(int32& Value : Parent) Value=INDEX_NONE;
   const int32 Start=(Rows/2)*Columns;
   Parent[Start]=Start;
   TArray<int32> Stack;
   Stack.Reserve(Cells);
   Stack.Add(Start);
   FRandomStream MazeRandom(7139+I*389);
   while(!Stack.IsEmpty())
   {
    const int32 Cell=Stack.Last();
    const int32 X=Cell%Columns, Y=Cell/Columns;
    int32 Neighbors[4], Available=0;
    if(X>0 && Parent[Cell-1]==INDEX_NONE) Neighbors[Available++]=Cell-1;
    if(X+1<Columns && Parent[Cell+1]==INDEX_NONE) Neighbors[Available++]=Cell+1;
    if(Y>0 && Parent[Cell-Columns]==INDEX_NONE) Neighbors[Available++]=Cell-Columns;
    if(Y+1<Rows && Parent[Cell+Columns]==INDEX_NONE) Neighbors[Available++]=Cell+Columns;
    if(Available==0)
    {
     Stack.Pop();
     continue;
    }
    const int32 Next=Neighbors[MazeRandom.RandRange(0,Available-1)];
    Parent[Next]=Cell;
    Depth[Next]=Depth[Cell]+1;
    Stack.Add(Next);
   }

   int32 ExitCell=Columns-1;
   for(int32 Row=1;Row<Rows;++Row)
   {
    const int32 EdgeCell=(Row+1)*Columns-1;
    if(Depth[EdgeCell]>Depth[ExitCell]) ExitCell=EdgeCell;
   }
   TArray<int32> BossRoute;
   for(int32 Cell=ExitCell;Cell!=Start;Cell=Parent[Cell]) BossRoute.Add(Cell);
   BossRoute.Add(Start);

   // GameMode is hidden in game, which left its wall collision active but invisible.
   auto* MazeActor=GetWorld()->SpawnActor<AActor>();
   UHierarchicalInstancedStaticMeshComponent* Walls=nullptr;
   if(MazeActor)
   {
    MazeActor->SetActorHiddenInGame(false);
    Walls=NewObject<UHierarchicalInstancedStaticMeshComponent>(MazeActor,TEXT("VisibleMazeWalls"));
    MazeActor->SetRootComponent(Walls);
    MazeActor->AddInstanceComponent(Walls);
    Walls->SetStaticMesh(Cube);
    if(Stone) Walls->SetMaterial(0,Stone);
    Walls->SetCollisionProfileName(TEXT("BlockAll"));
    Walls->SetMobility(EComponentMobility::Static);
    Walls->RegisterComponent();
   }
   const auto Wall=[&](FVector Floor,FVector Size)
   {
    const FVector Position=Floor+FVector(0,0,CeilingHeight*.5);
    if(Walls) Walls->AddInstance(FTransform(FRotator::ZeroRotator,Position,Size/100.),true);
    else Block(Position,Size,Stone);
   };
   const int32 FirstTurn=Start+2;
   const int32 NextCell=FirstTurn+(I%2==0?Columns:-Columns);
   const auto IsEntranceEdge=[Start,FirstTurn,NextCell](int32 A,int32 B)
   {
    return (B==A+1 && (A==Start || A==Start+1)) ||
      (A==FMath::Min(FirstTurn,NextCell) && B==FMath::Max(FirstTurn,NextCell));
   };
   const auto Connected=[&](int32 A,int32 B)
   {
    return Parent[A]==B || Parent[B]==A || IsEntranceEdge(FMath::Min(A,B),FMath::Max(A,B));
   };
   int32 SecretCount=0;
   for(int32 Cell=0;Cell<Cells;++Cell)
   {
    const FVector Floor=CellPoint(Cell);
    if(Cell%Columns+1<Columns && !Connected(Cell,Cell+1))
    {
     const FVector Edge=Floor+FVector(CellWidth*.5,0,0);
     const bool bSecret=SecretCount<3 && Depth[Cell]>10 && FMath::Abs(Depth[Cell]-Depth[Cell+1])>12;
     if(bSecret)
     {
      constexpr double DoorWidth=290.;
      const double Wing=(CellDepth-DoorWidth)*.5;
      for(double Side : {-1.,1.})
       Wall(Edge+FVector(0,Side*(DoorWidth+Wing)*.5,0),FVector(WallThickness,Wing,CeilingHeight));
      auto* Secret=GetWorld()->SpawnActor<AStaticMeshActor>(Edge+FVector(0,0,CeilingHeight*.5),FRotator::ZeroRotator);
      if(Secret)
      {
       auto* Panel=Secret->GetStaticMeshComponent();
       Panel->SetMobility(EComponentMobility::Movable);
       Panel->SetStaticMesh(Cube);
       Panel->SetMaterial(0,Stone);
       Panel->SetCollisionProfileName(TEXT("BlockAll"));
       Secret->SetActorScale3D(FVector(WallThickness-12.,DoorWidth,CeilingHeight)/100.);
       Secret->Tags.Add(TEXT("DungeonSecret"));
       Secret->Tags.Add(FName(*FString::FromInt(I*3+SecretCount)));
       ++SecretCount;
       // A small stone inlay is the only clue on either side of the seam.
       for(double Side : {-1.,1.})
        Block(Edge+FVector(Side*38,0,126),FVector(8,26,26),Mark);
      }
      else Wall(Edge,FVector(WallThickness,DoorWidth,CeilingHeight));
     }
     else Wall(Edge,FVector(WallThickness,CellDepth,CeilingHeight));
    }
    if(Cell/Columns+1<Rows && !Connected(Cell,Cell+Columns))
     Wall(Floor+FVector(0,CellDepth*.5,0),FVector(CellWidth,WallThickness,CeilingHeight));
    // Partial walls make doorways between selected cells, leaving 3.4 m clear.
    constexpr double Opening=340.;
    if((Cell+I)%4==0 && Cell%Columns+1<Columns && Connected(Cell,Cell+1) && !IsEntranceEdge(Cell,Cell+1))
    {
     const double Wing=(CellDepth-Opening)*.5;
     for(double Side : {-1.,1.})
      Wall(Floor+FVector(CellWidth*.5,Side*(Opening+Wing)*.5,0),FVector(WallThickness,Wing,CeilingHeight));
    }
    if((Cell+I*3)%5==0 && Cell/Columns+1<Rows && Connected(Cell,Cell+Columns) && !IsEntranceEdge(Cell,Cell+Columns))
    {
     const double Wing=(CellWidth-Opening)*.5;
     for(double Side : {-1.,1.})
      Wall(Floor+FVector(Side*(Opening+Wing)*.5,CellDepth*.5,0),FVector(Wing,WallThickness,CeilingHeight));
    }

    // Light junctions and vary side alcoves without marking the solution path.
    int32 Degree=0;
    if(Cell%Columns>0 && Connected(Cell,Cell-1)) ++Degree;
    if(Cell%Columns+1<Columns && Connected(Cell,Cell+1)) ++Degree;
    if(Cell/Columns>0 && Connected(Cell,Cell-Columns)) ++Degree;
    if(Cell/Columns+1<Rows && Connected(Cell,Cell+Columns)) ++Degree;
    if(Degree>=3 || (Cell+I*7)%9==0)
     PassageLamp(Floor+FVector(0,0,480),Degree>=3?Definition.Accent:FLinearColor(1.f,.50f,.23f));
    if(Degree==1 && (Cell<Start || Cell>FirstTurn) && Cell!=NextCell && Cell!=ExitCell && (Cell+I)%3==0)
     Block(Floor+FVector(0,0,29),FVector(148,148,58),Mark);
   }

   PassageLamp(CellPoint(Start)+FVector(0,0,480),FLinearColor(1.f,.58f,.31f));
   PassageLamp(CellPoint(Start+1)+FVector(0,0,480),FLinearColor(1.f,.58f,.31f));
   const FVector Passage=CellPoint(FirstTurn);
   const FVector TurningCell=CellPoint(NextCell);
   PassageLamp(FMath::Lerp(Passage,TurningCell,.6)+FVector(0,0,480),FLinearColor(1.f,.62f,.33f));
   Sign(FMath::Lerp(Passage,TurningCell,.45)+FVector(0,0,305),TEXT("Maze passage"),26);
   const double DoorY=CellPoint(ExitCell).Y;
   constexpr double DoorHalfWidth=180.;
   const double SouthSpan=DoorY-DoorHalfWidth-SouthEdge;
   const double NorthSpan=Center.Y+RoomHalfWidth-DoorY-DoorHalfWidth;
   Wall(FVector(MazeEnd,SouthEdge+SouthSpan*.5,Center.Z),FVector(WallThickness,SouthSpan,CeilingHeight));
   Wall(FVector(MazeEnd,DoorY+DoorHalfWidth+NorthSpan*.5,Center.Z),FVector(WallThickness,NorthSpan,CeilingHeight));
   Block(FVector(MazeEnd,DoorY,Center.Z+(CeilingHeight+310)*.5),
      FVector(WallThickness,2*DoorHalfWidth,CeilingHeight-310),Stone);
   Sign(FVector(MazeEnd-WallThickness*.5-3,DoorY,Center.Z+355),TEXT("Boss chamber"),30);

   const double ArenaLength=Center.X+RoomHalfLength-MazeEnd;
   const FVector Arena(MazeEnd+ArenaLength*.56,Center.Y,Center.Z);
   for(double Side : {-1.,1.})
   {
    for(double Along : {-.27,0.,.27})
    {
     const FVector Row=Arena+FVector(ArenaLength*Along,Side*RoomHalfWidth*.49,0);
     Wall(Row,FVector(160,160,CeilingHeight));
     PassageLamp(Row+FVector(0,-Side*150,475),Definition.Accent);
    }
   }
   PassageLamp(Arena+FVector(0,0,525),FLinearColor(1.f,.62f,.36f));

   const FVector Encounter[]={
    CellPoint(BossRoute[BossRoute.Num()*3/4])+FVector(0,0,95),
    CellPoint(BossRoute[BossRoute.Num()/2])+FVector(0,0,95),
    CellPoint(BossRoute[BossRoute.Num()/4])+FVector(0,0,95),
    Arena+FVector(0,0,95)
   };
   for(int32 Slot=0;Slot<UE_ARRAY_COUNT(Encounter);++Slot)
   {
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    if(auto* Enemy=GetWorld()->SpawnActor<ACryptMonster>(Encounter[Slot],FRotator::ZeroRotator,Spawn))
    {
     Enemy->SaveId=EnemyId(I,Slot);
     Enemy->ConfigureCreature(Definition.Enemies[Slot],I,Slot==UE_ARRAY_COUNT(Encounter)-1);
    }
   }
  }
 }

 // Castle masonry uses world-space projection across the different wall sizes.
 UStaticMesh* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
 const auto Masonry=[this,Stone](const FVector& Location,const FVector& Scale,const FRotator& Rotation=FRotator::ZeroRotator,UStaticMesh* Mesh=nullptr)
 {
  PlaceBlock(Location+FVector(CryptWorld::CastleOffsetX,0,0),Scale,Rotation,Stone,Mesh);
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
 PlaceBlock((FVector(1000,3200,-57)+RuinOffset),FVector(36,30,1),FRotator::ZeroRotator,Stone);
 for(int32 i=0;i<8;++i)
 {
  const float X=-600.f+(i%4)*950.f;
  const float Y=1800.f+(i/4)*2800.f;
  PlaceBlock((FVector(X,Y,160)+RuinOffset),FVector(1.5f,1.5f,4.5f));
 }
 PlaceBlock((FVector(1000,4650,180)+RuinOffset),FVector(20,1.5f,5));
 PlaceBlock((FVector(-700,3200,180)+RuinOffset),FVector(1.5f,30,5));
 PlaceBlock((FVector(2700,3200,180)+RuinOffset),FVector(1.5f,30,5));
 for(int32 i=0;i<5;++i)
  if(ACryptMonster* Guardian=GetWorld()->SpawnActor<ACryptMonster>((FVector(200+i*380.f,3000.f+(i%2)*300.f,120.f)+RuinOffset),FRotator::ZeroRotator))
  {
   Guardian->SaveId=100+i;
   Guardian->ConfigureCreature(1);
  }
 // Roadside timber fences, lantern posts, rubble, and a village square.
 for(int32 i=0;i<26;++i)
 {
  const float X=-2700.f+i*275.f;
  if(X>=450.f && X<=4025.f) continue;
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
 // A cobbled village square with market stalls and stone benches.
 // Square paving top = +19 cm, above road top = -2 cm; avoid near-coplanar surfaces.
 PlaceBlock(FVector(2400,0,12)+TownOffset,FVector(22,18,.14f),FRotator::ZeroRotator,RoadMaterial);
 for(int32 i=0;i<9;++i)
 {
  const float X=1600.f+(i%3)*750.f;
  const float Y=-650.f+(i/3)*650.f;
  PlaceBlock(FVector(X,Y,18)+TownOffset,FVector(1.4f,1.4f,.22f),FRotator::ZeroRotator,RoadMaterial);
 }
 for(int32 i=0;i<4;++i)
 {
  const float X=1550.f+i*580.f;
  PlaceBlock(FVector(X,1000,110)+TownOffset,FVector(3.1f,1.5f,.25f));
  for(int32 j=-1;j<=1;j+=2)
   PlaceBlock(FVector(X+j*125.f,1000,60)+TownOffset,FVector(.22f,.22f,1.25f));
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
  PlaceBlock((FVector(X,2600,90)+RuinOffset),FVector(.8f,9.f,2.7f));
  PlaceBlock((FVector(X,3750,90)+RuinOffset),FVector(.8f,9.f,2.7f));
 }
 for(int32 i=0;i<22;++i)
 {
  const float X=-450.f+(i*317%2750);
  const float Y=1900.f+(i*491%2400);
  PlaceBlock((FVector(X,Y,12)+RuinOffset),FVector(.35f+(i%3)*.2f,.5f,.35f));
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
  SpawnQuestActor(Index,true,FVector(1450.f+400.f*Index,185.f,19.f)+TownOffset);

 const auto SpawnTerrainObjective=[&](int32 Quest,float X,float Y,int32 ObjectiveId)
 {
  SpawnQuestActor(Quest,false,FVector(X,Y,TerrainHeight(X,Y)),ObjectiveId);
 };
 SpawnTerrainObjective(1,-430.f,-2000.f,10);
 SpawnTerrainObjective(1,250.f,-2360.f,11);
 SpawnTerrainObjective(1,720.f,-2750.f,12);
 SpawnQuestActor(2,false,(FVector(1020,3110,-7)+RuinOffset),20);
 SpawnTerrainObjective(3,-3300.f,-2780.f,30);
 SpawnTerrainObjective(4,2450.f,-2330.f,40);
 PlantOverworld(GetWorld());
}
