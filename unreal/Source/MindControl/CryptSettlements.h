#pragma once

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

namespace CryptSettlements
{
 struct FResident
 {
  TArray<FVector> Route;
  float Shift;
  float Height;
  bool bPet;
  int32 Job;
 };

 inline void Build(UWorld* World, UMaterialInterface* Grass, UMaterialInterface* Stone,
                   UMaterialInterface* Cobble, UMaterialInterface* Dirt, float Ground)
 {
  auto* District = World->SpawnActor<AActor>();
  if (!District) return;
  auto* Root = NewObject<USceneComponent>(District);
  District->SetRootComponent(Root);
  District->AddInstanceComponent(Root);
  Root->RegisterComponent();

  auto* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
  auto* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
  auto* Basic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
  const auto Color = [&](FLinearColor Value) -> UMaterialInterface*
  {
   if (!Basic) return nullptr;
   auto* Material = UMaterialInstanceDynamic::Create(Basic, District);
   Material->SetVectorParameterValue(TEXT("Color"), Value);
   return Material;
  };
  auto* Oak = Color(FLinearColor(.22f,.1f,.043f));
  auto* Cloth = Color(FLinearColor(.32f,.12f,.075f));
  auto* Skin = Color(FLinearColor(.57f,.32f,.20f));
  auto* Dark = Color(FLinearColor(.056f,.06f,.064f));
  auto* Glass = Color(FLinearColor(.20f,.32f,.37f));

  const auto Batch = [&](UStaticMesh* Mesh, UMaterialInterface* Material, bool bCollision, bool bMoving = false) -> UInstancedStaticMeshComponent*
  {
   auto* Component = bMoving
    ? NewObject<UInstancedStaticMeshComponent>(District)
    : NewObject<UHierarchicalInstancedStaticMeshComponent>(District);
   District->AddInstanceComponent(Component);
   Component->SetupAttachment(Root);
   Component->SetMobility(bMoving ? EComponentMobility::Movable : EComponentMobility::Static);
   Component->SetStaticMesh(Mesh);
   if (Material) Component->SetMaterial(0, Material);
   Component->SetCollisionProfileName(bCollision ? TEXT("BlockAll") : TEXT("NoCollision"));
   Component->SetCanEverAffectNavigation(false);
   Component->SetCastShadow(!bMoving);
   if (bMoving) Component->SetCullDistances(3500, 5000);
   Component->RegisterComponent();
   return Component;
  };
  auto* GroundBatch = Batch(Cube, Grass, true);
  auto* Road = Batch(Cube, Cobble, false);
  auto* Wall = Batch(Cube, Stone, true);
  auto* Tile = Batch(Cube, Dirt, true);
  auto* Timber = Batch(Cube, Oak, true);
  auto* Fabric = Batch(Cube, Cloth, false);
  auto* Windows = Batch(Cube, Glass, false);
  auto* Fence = Batch(Cube, nullptr, true);
  const auto Box = [](UInstancedStaticMeshComponent* Batch, FVector At, FVector Size,
                      FRotator Rotation = FRotator::ZeroRotator)
  {
   Batch->AddInstance(FTransform(Rotation, At, Size / 100.));
  };

  constexpr float Spacing = 3500;
  constexpr float EntryX = -6600;
  constexpr float StartX = -8850;
  constexpr float MinX = -24900;
  constexpr float HalfY = 5850;
  Box(GroundBatch, FVector((EntryX + MinX) / 2, 0, Ground - 42), FVector(EntryX - MinX, HalfY * 2, 84));
  Box(Road, FVector((EntryX + MinX) / 2, 0, Ground + 1), FVector(EntryX - MinX, 410, 2));
  for (const float Y : {-HalfY, HalfY})
   Box(Fence, FVector((EntryX + MinX) / 2, Y, Ground + 180), FVector(EntryX - MinX, 24, 360));
  Box(Fence, FVector(MinX, 0, Ground + 180), FVector(24, HalfY * 2, 360));
  for (const float Y : {-3070.f,3070.f})
   Box(Fence, FVector(EntryX - 4, Y, Ground + 180), FVector(20, 5550, 360));

  // Buildings face their local street. Door and window bays are actual openings.
  const auto Building = [&](FVector Base, float W, float D, int32 Style, bool bWork)
  {
   const float H = 270.f + 16.f * (Style % 4);
   const auto Part = [&](auto* Mesh, float X, float Y, float Z, float SX, float SY, float SZ)
   {
    Box(Mesh, Base + FVector(X,Y,Z), FVector(SX,SY,SZ));
   };
   Part(Timber,0,0,5,W,D,10);
   Part(Wall,0,D/2,H/2,W,18,H);
   for (const float Side : {-1.f,1.f})
   {
    Part(Wall,Side*W/2,0,H/2,18,D,H);
    Part(Wall,Side*(W/4+32),-D/2,47,W/2-64,18,94);
    Part(Wall,Side*(W/4+32),-D/2,H-35,W/2-64,18,70);
    Part(Wall,Side*(W/2-31),-D/2,150,62,18,112);
    Part(Windows,Side*(W/4+23),-D/2+3,150,FMath::Max(40.f,W/2-142),5,82);
    Part(Timber,Side*(W/4+23),-D/2-3,150,7,10,92);
    const float Rise = 75.f + 15.f * (Style % 4);
    const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Rise,D/2));
    Box(Tile, Base + FVector(0,Side*D/4,H+Rise/2),
        FVector(W+48,FMath::Sqrt(FMath::Square(D/2)+FMath::Square(Rise))+35,14),
        FRotator(0,0,Side*Angle));
    Part(Timber,Side*W*.30f,D*.16f,47,65,70,12);
    Part(Timber,Side*W*.30f,D*.16f,24,7,7,45);
   }
   Part(Wall,0,-D/2,H-19,128,18,38);
   Part(Tile,W*.32f,D*.19f,H+60,62,60,170);
   Part(Timber,0,D*.10f,77,105,73,10);
   for (float X : {-43.f,43.f})
    for (float Y : {D*.10f-28.f,D*.10f+28.f})
     Part(Timber,X,Y,38,8,8,76);
   Part(Timber,-W*.27f,D*.34f,57,74,47,108);
   if (bWork)
   {
    Part(Timber,W*.28f,D*.31f,83,122,63,14);
    Part(Tile,W*.28f,D*.31f,37,109,49,70);
    Part(Timber,-W*.24f,-D*.24f,26,48,48,52);
    Part(Timber,-W*.24f,-D*.24f,61,42,42,39);
   }
   else
   {
    Part(Timber,W*.28f,D*.23f,31,103,155,42);
    Part(Fabric,W*.28f,D*.23f,55,98,148,16);
    Part(Fabric,W*.28f,D*.36f,65,80,33,15);
    Part(Fabric,-W*.26f,-D*.20f,13,76,64,8);
    Part(Tile,-W*.29f,D*.12f,29,62,48,40);
    Part(Timber,-W*.28f,-D*.34f,42,70,48,77);
    Part(Tile,-W*.28f,-D*.34f,83,73,50,8);
    Part(Timber,W*.27f,-D*.31f,30,63,40,48);
    Part(Fabric,W*.27f,-D*.31f,56,58,39,9);
   }
  };

  const TCHAR* TownNames[] = {
   TEXT("Alderbrook"), TEXT("Briar Glen"), TEXT("Copper Hollow"), TEXT("Dunmere"),
   TEXT("Elder Ford"), TEXT("Fernwick"), TEXT("Greybank"), TEXT("Hawthorn"),
   TEXT("Ironwood"), TEXT("Juniper Hill"), TEXT("Kestrel Reach"), TEXT("Larkspur"),
   TEXT("Millstone"), TEXT("Northwyck"), TEXT("Oakcross"), TEXT("Pinehaven"),
   TEXT("Quarry End"), TEXT("Redwater"), TEXT("Stonefield"), TEXT("Willowmere")
  };
  TArray<FResident> Residents;
  for (int32 Column = 0; Column < 5; ++Column)
  {
   const float X = StartX - Column * Spacing;
   Box(Road, FVector(X+1100,0,Ground+1), FVector(340,HalfY*2,2));
   for (int32 Row = 0; Row < 4; ++Row)
   {
    const int32 Town = Column * 4 + Row;
    const float Y = (Row - 1.5f) * 2640;
    const float Street = Y - 480;
    Box(Timber,FVector(X+870,Street+210,Ground+81),FVector(13,13,162));
    auto* Sign = NewObject<UTextRenderComponent>(District);
    District->AddInstanceComponent(Sign);
    Sign->SetupAttachment(Root);
    Sign->SetRelativeLocation(FVector(X+870,Street+199,Ground+142));
    Sign->SetRelativeRotation(FRotator(0,-90,0));
    Sign->SetHorizontalAlignment(EHTA_Center);
    Sign->SetWorldSize(28);
    Sign->SetText(FText::FromString(TownNames[Town]));
    Sign->RegisterComponent();
    Box(Road, FVector(X, Street, Ground+2), FVector(2560,340,2));
    Box(Road, FVector(X+1100,Y+170,Ground+2), FVector(230,1300,2));
    const float WorkDepth = 490 + 35 * (Town % 4);
    for (int32 Shop = 0; Shop < 2; ++Shop)
    {
     const float ShopY = Y + 260 + Shop * 720;
     Building(FVector(X+800, ShopY, Ground), 440 + 42*(Town % 3), WorkDepth, Town+Shop, true);
     Box(Road, FVector(X+940,ShopY-WorkDepth/2-65,Ground+2),FVector(440,138,2));
    }
    for (int32 Home = 0; Home < (Town < 10 ? 3 : 2); ++Home)
    {
     const float HomeX = X - 940 + 620*Home;
     const float Depth = 355 + 48*((Town+Home)%5);
     const float HomeY = Street+400+Depth/2;
     Building(FVector(HomeX, HomeY, Ground), 330 + 45*((Town+2*Home)%5), Depth, Town+Home, false);
     Box(Road, FVector(HomeX,Street+276,Ground+2),FVector(120,260,2));
     for (int32 Member = 0; Member < 4; ++Member)
     {
      const float JobY = Y+260+(Member%2)*720-WorkDepth/2;
      const FVector AtHome(HomeX + (Member-1.5f)*24,HomeY-Depth/2+90,Ground+10);
      const FVector Door(HomeX, HomeY-Depth/2-25,Ground);
      const FVector Curb(HomeX,Street+34*Member,Ground);
      const FVector Junction(X+1100,Curb.Y,Ground);
      const FVector ShopCorner(X+1100,JobY-76,Ground);
      const FVector ShopDoor(X+800,JobY-76,Ground);
      const FVector Work(X+800,JobY+WorkDepth*.56f,Ground+10);
      FResident Resident;
      Resident.bPet = Member == 3;
      Resident.Job = (Town+Member)%4;
      Resident.Height = Member == 2 ? 117.f : 165.f + 7.f * ((Town+Member)%3);
      Resident.Shift = 3.f*Home + 2.f*Member + (Town%5);
      Resident.Route = Member < 2
       ? TArray<FVector>{AtHome,Door,Curb,Junction,ShopCorner,ShopDoor,Work}
       : TArray<FVector>{AtHome,Door,Curb,Curb+FVector(160,0,0),Door};
      Residents.Add(MoveTemp(Resident));
     }
    }
   }
  }

  auto* Heads = Batch(Sphere, Skin, false, true);
  auto* Bodies = Batch(Cube, Cloth, false, true);
  auto* Limbs = Batch(Cube, Dark, false, true);
  auto* Pets = Batch(Sphere, Oak, false, true);
  for (const FResident& Resident : Residents)
  {
   if (Resident.bPet)
   {
    for (int32 Piece = 0; Piece < 7; ++Piece) Pets->AddInstance(FTransform::Identity);
   }
   else
   {
    Heads->AddInstance(FTransform::Identity);
    Bodies->AddInstance(FTransform::Identity);
    for (int32 Piece = 0; Piece < 4; ++Piece) Limbs->AddInstance(FTransform::Identity);
   }
  }
  TBitArray<> WasVisible(false, Residents.Num());
  auto Update = [World, Residents = MoveTemp(Residents), Heads, Bodies, Limbs, Pets,
                       WasVisible = MoveTemp(WasVisible)]() mutable
  {
   const APawn* Player = World->GetFirstPlayerController() ? World->GetFirstPlayerController()->GetPawn() : nullptr;
   const FVector Eye = Player ? Player->GetActorLocation() : FVector::ZeroVector;
   const float Time = World->GetTimeSeconds();
   int32 Person = 0, Pet = 0, ResidentIndex = 0;
   bool bChanged = false;
   for (const FResident& Resident : Residents)
   {
    const auto& Route = Resident.Route;
    const float Hour = FMath::Fmod(Time + 74.f + Resident.Shift, 240.f);
    const float RoutePhase = Hour < 60 ? 0.f : Hour < 104 ? (Hour-60)/44 :
                             Hour < 168 ? 1.f : Hour < 211 ? (211-Hour)/43 : 0.f;
    float Distance = 0;
    for (int32 I = 1; I < Route.Num(); ++I) Distance += FVector::Dist(Route[I-1],Route[I]);
    float Remaining = Distance*RoutePhase;
    int32 Segment = 1;
    while (Segment < Route.Num()-1 && Remaining > FVector::Dist(Route[Segment-1],Route[Segment]))
     Remaining -= FVector::Dist(Route[Segment-1],Route[Segment++]);
    const FVector Delta = Route[Segment]-Route[Segment-1];
    const FVector Position = Route[Segment-1] + Delta.GetClampedToMaxSize(Remaining);
    const bool bMoving = RoutePhase > 0.f && RoutePhase < 1.f;
    const bool bWorking = Route.Num() == 7 && RoutePhase == 1.f;
    const FRotator Facing(0,Delta.Rotation().Yaw+(Hour >=168 && Hour <211 ? 180 : 0),0);
    const bool bVisible = FVector::DistSquared(Eye,Position)<FMath::Square(4900.f);
    const int32 Index = ResidentIndex++;
    if (WasVisible[Index] == bVisible && !bVisible && Time > .2f)
    {
     if (Resident.bPet) ++Pet;
     else ++Person;
     continue;
    }
    WasVisible[Index] = bVisible;
    bChanged = true;
    const float Swing = bMoving ? FMath::Sin(Time*10 + Resident.Shift) * 24 : 0;
    const auto Pose = [&](auto* Batch, int32 Index, FVector Offset, FVector Size, float Pitch = 0)
    {
     Batch->UpdateInstanceTransform(Index,FTransform(FRotator(Pitch,Facing.Yaw,0),
        Position+Facing.RotateVector(Offset),bVisible ? Size/100. : FVector::ZeroVector),false,false,true);
    };
    if (Resident.bPet)
    {
     Pose(Pets,Pet*7,FVector(0,0,32),FVector(63,29,27));
     Pose(Pets,Pet*7+1,FVector(33,0,42),FVector(25,23,24));
     Pose(Pets,Pet*7+2,FVector(-31,0,39),FVector(29,8,8),-25);
     for (int32 I=0;I<4;++I)
      Pose(Pets,Pet*7+3+I,FVector(I<2?18:-18,I%2?10:-10,15),FVector(9,9,27),I%2?Swing:-Swing);
     ++Pet;
    }
    else
    {
     const float H = Resident.Height;
     Pose(Heads,Person,FVector(0,0,H*.91f),FVector(24,24,28));
     Pose(Bodies,Person,FVector(0,0,H*.67f),FVector(24,38,H*.33f));
     for (int32 Side=0;Side<2;++Side)
     {
      const float Sign = Side ? 1.f : -1.f;
      Pose(Limbs,Person*4+Side,FVector(0,Sign*10,H*.21f),FVector(14,15,H*.43f),Sign*Swing);
      const float Task = bWorking ? -36.f - 20.f*FMath::Sin(Time*(2+Resident.Job)+Side) : -Sign*Swing;
      Pose(Limbs,Person*4+2+Side,FVector(0,Sign*24,H*.63f),FVector(12,12,H*.36f),Task);
     }
     ++Person;
    }
   }
   if (bChanged)
    for (auto* Component : {Heads,Bodies,Limbs,Pets}) Component->MarkRenderStateDirty();
  };
  FTimerHandle Timer;
  World->GetTimerManager().SetTimer(Timer,FTimerDelegate::CreateWeakLambda(District, Update),.083f,true);
  Update();
 }
}
