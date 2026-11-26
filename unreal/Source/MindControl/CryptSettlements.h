#pragma once

#include "CryptWorldLayout.h"
#include "CryptHero.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Sound/SoundBase.h"
#include "Misc/ConfigCacheIni.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

namespace CryptSettlements
{
 // The existing western settlement grid is centered on Y=0.
 inline constexpr float DistrictNorth = 0.f;

 struct FResident
 {
  TArray<FVector> Route;
  float Shift;
  float Height;
  bool bPet;
  int32 Job;
  int32 Appearance;
  int32 MeshSlot = INDEX_NONE;
  int32 SpellSlot = INDEX_NONE;
 };

 inline void SpawnMerchant(UWorld* World,FVector Floor,int32 Id)
 {
  auto* Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Characters/Town/Townsperson.Townsperson"));
  if(!Mesh) return;
  auto* Merchant=World->SpawnActor<AStaticMeshActor>(Floor,FRotator(0,-90,0));
  if(!Merchant) return;
  auto* Body=Merchant->GetStaticMeshComponent();
  Body->SetMobility(EComponentMobility::Movable);
  Body->SetStaticMesh(Mesh);
  Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  const auto Bounds=Mesh->GetBounds();
  const double Scale=174./FMath::Max(1.,2.*Bounds.BoxExtent.Z);
  Merchant->SetActorScale3D(FVector(Scale));
  Merchant->AddActorWorldOffset(FVector(0,0,(Bounds.BoxExtent.Z-Bounds.Origin.Z)*Scale));
  Merchant->Tags.Add(TEXT("TownMerchant"));
  Merchant->Tags.Add(FName(*FString::FromInt(Id)));
  auto* Label=NewObject<UTextRenderComponent>(Merchant);
  Merchant->AddInstanceComponent(Label);
  Label->SetupAttachment(Body);
  Label->SetRelativeLocation(FVector(0,0,203./Scale));
  Label->SetHorizontalAlignment(EHTA_Center);
  Label->SetWorldSize(25./Scale);
  Label->SetText(FText::FromString(TEXT("Merchant")));
  Label->RegisterComponent();
 }

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
  auto* LightningColor = Color(FLinearColor(.30f,.73f,1.f));
  auto* FireColor = Color(FLinearColor(1.f,.23f,.025f));
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
  Wall->ComponentTags.Add(TEXT("FlammableBuilding"));
  Tile->ComponentTags.Add(TEXT("FlammableBuilding"));
  Timber->ComponentTags.Add(TEXT("FlammableBuilding"));
  auto* Fabric = Batch(Cube, Cloth, false);
  auto* Windows = Batch(Cube, Glass, false);
  auto* Fence = Batch(Cube, nullptr, true);
  auto* Trim = Batch(Cube, Oak, false);
  auto* Sill = Batch(Cube, Stone, false);
  auto* PlanterLeaves = Batch(Sphere, Color(FLinearColor(.17f,.31f,.09f)), false);
  auto* Metal = Batch(Cube, Color(FLinearColor(.10f,.085f,.07f)), false);
  auto* LanternGlass = Batch(Cube, Color(FLinearColor(.95f,.64f,.24f)), false);
  UInstancedStaticMeshComponent* Shutters[] = {
   Batch(Cube, Color(FLinearColor(.21f,.34f,.24f)), false),
   Batch(Cube, Color(FLinearColor(.35f,.14f,.10f)), false),
   Batch(Cube, Color(FLinearColor(.19f,.28f,.40f)), false)
  };
  UInstancedStaticMeshComponent* RoofPanels[] = {
   Batch(Cube, Color(FLinearColor(.30f,.14f,.092f)), true),
   Batch(Cube, Color(FLinearColor(.18f,.22f,.255f)), true),
   Batch(Cube, Color(FLinearColor(.29f,.22f,.15f)), true)
  };
  for (auto* Details : {Trim,Sill,PlanterLeaves,Metal,LanternGlass,Shutters[0],Shutters[1],Shutters[2]})
  {
   Details->SetCullDistances(5000,7200);
   Details->SetCastShadow(false);
  }
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
  Box(Road, FVector((EntryX + MinX) / 2, 0, Ground + 2), FVector(EntryX - MinX, 410, 12));
  for (const float Y : {-HalfY, HalfY})
   Box(Fence, FVector((EntryX + MinX) / 2, Y, Ground + 180), FVector(EntryX - MinX, 24, 360));
  Box(Fence, FVector(MinX, 0, Ground + 180), FVector(24, HalfY * 2, 360));
  for (const float Y : {-3070.f,3070.f})
   Box(Fence, FVector(EntryX - 4, Y, Ground + 180), FVector(20, 5550, 360));

  // Buildings face their local street. Door and window bays are actual openings.
  const auto Building = [&](FVector Base, float W, float D, int32 Style, bool bWork, float Yaw = 0.f, bool bSecret = false)
  {
   const float H = 270.f + 16.f * (Style % 4);
   const FRotator Facing(0,Yaw,0);
   const auto Part = [&](auto* Batch, float X, float Y, float Z, float SX, float SY, float SZ,
                         FRotator Rotation = FRotator::ZeroRotator)
   {
    Box(Batch,Base+Facing.RotateVector(FVector(X,Y,Z)),FVector(SX,SY,SZ),
        (Facing.Quaternion()*Rotation.Quaternion()).Rotator());
   };
   auto* Shutter=Shutters[Style%UE_ARRAY_COUNT(Shutters)];
   auto* Roof=RoofPanels[Style%UE_ARRAY_COUNT(RoofPanels)];
   const float Rise = 75.f + 15.f * (Style % 4);
   const float RoofSlope=FMath::Sqrt(FMath::Square(D/2)+FMath::Square(Rise));
   const float Angle=FMath::RadiansToDegrees(FMath::Atan2(Rise,D/2));
   const float Front=-D/2;
   Part(Wall,0,D/2,-4,W+8,22,29);
   Part(Sill,0,Front-17,4,118,36,12);
   Part(Trim,0,Front-10,H-12,W+10,13,14);
   Part(Trim,0,D/2+9,H-12,W+10,13,14);
   Part(Trim,0,0,H+Rise+6,W+56,18,13);
   for (const float Edge : {-1.f,1.f})
   {
    for (int32 Tier=0;Tier<5;++Tier)
     Part(Wall,Edge*W/2,0,H+(Tier+.5f)*Rise/6,18,D*(1.f-(Tier+1)/6.f),Rise/6);
    Part(Trim,Edge*(W/2+9),0,H+Rise*.45f,12,12,Rise*.9f);
    Part(Sill,Edge*(W/2+4),0,19,18,D+10,35);
    Part(Windows,Edge*(W/2+10),D*.10f,152,5,80,94);
    Part(Trim,Edge*(W/2+14),D*.10f,152,9,90,7);
    Part(Trim,Edge*(W/2+14),D*.10f,152,9,7,100);
    Part(Sill,Edge*(W/2+13),D*.10f,103,19,99,11);
   }
   Part(Timber,0,0,5,W,D,10);
   Part(Wall,0,D/2,H/2,W,18,H);
   for (const float Side : {-1.f,1.f})
   {
    Part(Wall,Side*W/2,0,H/2,18,D,H);
    const float WindowX=Side*(W/4+32);
    const float BayWidth=W/2-64;
    const float WindowWidth=FMath::Clamp(BayWidth-42.f,44.f,105.f);
    const float JambWidth=(BayWidth-WindowWidth)*.5f;
    Part(Wall,WindowX,Front,54,BayWidth,18,108);
    Part(Wall,WindowX,Front,(H+192)*.5f,BayWidth,18,H-192);
    for(float Jamb : {-1.f,1.f})
     Part(Wall,WindowX+Jamb*(WindowWidth+JambWidth)*.5f,Front,150,JambWidth,18,84);
    Part(Windows,WindowX,Front+5,150,WindowWidth,5,84);
    Part(Trim,WindowX,Front-7,150,7,10,84);
    const FRotator RoofTilt(0,0,Side*Angle);
    Part(Roof,0,Side*D/4,H+Rise/2,W+48,RoofSlope+35,14,RoofTilt);
    for (int32 Seam=1;Seam<=5;++Seam)
    {
     const float Row=Seam/6.f;
     Part(Trim,0,Side*D*.5f*Row,H+Rise*(1.f-Row)+9,W+46,5,5,RoofTilt);
    }
    Part(Trim,0,Side*(D/2+12),H-4,W+56,13,17);
    Part(Trim,WindowX,Front-9,150,WindowWidth+10,12,6);
    Part(Trim,WindowX,Front-8,194,WindowWidth+13,13,9);
    Part(Sill,WindowX,Front-13,102,WindowWidth+26,25,10);
    Part(Shutter,WindowX+Side*(WindowWidth*.5f+11),Front-12,149,19,8,87);
    Part(Trim,Side*(W/2-10),Front-8,H*.5f,13,13,H);
    Part(Trim,Side*63,Front-10,106,13,16,208);
    Part(Trim,Side*(W*.38f),Front-9,228,W*.20f,10,9,FRotator(Side*27,0,0));
    if(!bWork && Style%3!=0)
    {
     Part(Shutter,WindowX,Front-28,88,WindowWidth+14,24,21);
     for(float Offset : {-15.f,0.f,15.f})
      Part(PlanterLeaves,WindowX+Offset,Front-29,105+FMath::Abs(Offset)*.12f,21,20,18);
    }
    Part(Timber,Side*W*.30f,D*.16f,47,65,70,12);
    Part(Timber,Side*W*.30f,D*.16f,24,7,7,45);
   }
   Part(Wall,0,-D/2,H-19,128,18,38);
   if(bSecret)
   {
    const float CX=W*.32f,CY=D*.19f;
    for(float Side : {-1.f,1.f})
    {
     Part(Wall,CX+Side*61,CY,H+60,14,136,170);
     Part(Wall,CX,CY+Side*61,H+60,108,14,170);
    }
    // Freestanding street sign matching the town's timber signs.
    const float SignX=W*.40f;
    const float SignY=Front-110.f;
    Part(Timber,SignX,SignY,81,13,13,162);
    Part(Timber,SignX,SignY,142,145,12,40);
    // Solid cap provides a landing surface for the roof jump.
    Part(Timber,SignX,SignY,166,145,90,8);
    for(float Side : {-1.f,1.f})
    {
     auto* Sign=NewObject<UTextRenderComponent>(District);
     District->AddInstanceComponent(Sign);
     Sign->SetupAttachment(Root);
     Sign->SetRelativeLocation(Base+Facing.RotateVector(FVector(SignX,SignY+Side*8,142)));
     Sign->SetRelativeRotation(FRotator(0,Yaw+Side*90,0));
     Sign->SetHorizontalAlignment(EHTA_Center);
     Sign->SetVerticalAlignment(EVRTA_TextCenter);
     Sign->SetWorldSize(24);
     Sign->SetText(FText::FromString(TEXT("Cottage")));
     Sign->RegisterComponent();
    }
    auto* Door=NewObject<UStaticMeshComponent>(District);
    District->AddInstanceComponent(Door);
    Door->SetupAttachment(Root);
    Door->SetMobility(EComponentMobility::Movable);
    Door->SetStaticMesh(Cube);
    Door->SetMaterial(0,Oak);
    Door->SetRelativeTransform(FTransform(Facing,Base+Facing.RotateVector(FVector(0,Front,106)),FVector(1.26,.20,2.12)));
    Door->SetCollisionProfileName(TEXT("BlockAll"));
    Door->RegisterComponent();
    const FVector DoorPosition=Base+Facing.RotateVector(FVector(0,Front,106));
    Part(Timber,-W*.30f,-D*.10f,35,75,55,60);
    Part(LanternGlass,-W*.30f,-D*.10f,68,76,56,8);
    const FVector Mouth=Base+Facing.RotateVector(FVector(CX,CY,H+145));
    const FVector Landing=Base+Facing.RotateVector(FVector(0,-D*.12f,115));
    const FVector Chest=Base+Facing.RotateVector(FVector(-W*.30f,-D*.10f,100));
    FTimerHandle SecretTimer;
    World->GetTimerManager().SetTimer(SecretTimer,FTimerDelegate::CreateWeakLambda(District,[World,Mouth,Landing,Chest,Door,DoorPosition,Base,Facing,D,bEntered=false,bUnlocked=false]() mutable
    {
     auto* PC=World->GetFirstPlayerController();
     auto* Hero=PC?Cast<ACryptHero>(PC->GetPawn()):nullptr;
     if(!Hero) return;
     const FVector P=Hero->GetActorLocation();
     if(FVector::DistSquared2D(P,Mouth)<FMath::Square(48.) && P.Z>Mouth.Z+35 && P.Z<Mouth.Z+160 && Hero->GetVelocity().Z<0)
     {
      Hero->SetActorLocation(Landing,false,nullptr,ETeleportType::TeleportPhysics);
      Hero->GetCharacterMovement()->StopMovementImmediately();
      bEntered=true;
      if(auto* Sound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Environment/MindControl/Audio/ChimneyMagic.ChimneyMagic")))
       UGameplayStatics::PlaySound2D(Hero,Sound,Hero->GetAudioSetting(0)*Hero->GetAudioSetting(4));
     }
     if(bEntered) bUnlocked=true;
     if(bUnlocked && IsValid(Door))
     {
      const FVector Local=Facing.UnrotateVector(P-Base);
      const bool bExit=Local.Y>-D/2-100 && Local.Y<-D/2+120 && FMath::Abs(Local.X)<100 && Local.Z<240;
      Door->SetRelativeLocation(DoorPosition+(bExit?FVector(0,0,230):FVector::ZeroVector));
      Door->SetCollisionEnabled(bExit?ECollisionEnabled::NoCollision:ECollisionEnabled::QueryAndPhysics);
     }
     if(bEntered && FVector::DistSquared(P,Chest)<FMath::Square(125.))
     {
      bool bClaimed=false;
      if(GConfig) GConfig->GetBool(TEXT("MindControl.Secrets"),TEXT("FirstCottageChest"),bClaimed,GGameUserSettingsIni);
      if(!bClaimed)
      {
       Hero->AddTreasure(250);
       if(GConfig) { GConfig->SetBool(TEXT("MindControl.Secrets"),TEXT("FirstCottageChest"),true,GGameUserSettingsIni); GConfig->Flush(false,GGameUserSettingsIni); }
      }
      bEntered=false;
     }
    }),.05f,true);
   }
   else
   {
    Part(Wall,W*.32f,D*.19f,H+60,62,60,170);
    Part(Sill,W*.32f,D*.19f,H+145,76,73,12);
    Part(Metal,W*.32f,D*.19f,H+152,46,42,5);
   }
   Part(Trim,0,Front-10,211,139,16,17);
   Part(Metal,82,Front-19,181,12,19,42);
   Part(LanternGlass,82,Front-23,179,9,21,24);
   Part(Trim,-W*.33f,D*.40f,124,83,31,9);
   Part(Trim,-W*.33f,D*.40f,163,83,31,9);
   Part(Shutter,-W*.33f,D*.39f,136,21,19,23);
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
    Part(Trim,W*.28f,D*.41f,74,109,11,84);
    Part(Fabric,W*.28f,D*.19f,66,100,82,7);
    Part(Fabric,0,-D*.21f,11,95,99,2);
    Part(Sill,0,D*.43f,32,93,36,31);
    Part(Metal,0,D*.41f,53,47,27,12);
    Part(Fabric,-W*.26f,-D*.20f,13,76,64,8);
    Part(Tile,-W*.29f,D*.12f,29,62,48,40);
    Part(Timber,-W*.28f,-D*.34f,42,70,48,77);
    Part(Tile,-W*.28f,-D*.34f,83,73,50,8);
    Part(Timber,W*.27f,-D*.31f,30,63,40,48);
    Part(Fabric,W*.27f,-D*.31f,56,58,39,9);
   }
  };

  // Main hub cottages use the same furnished house kits and face the square.
  for(int32 Cottage=0;Cottage<4;++Cottage)
  {
   const float X=1120.f+Cottage*650.f;
   const float Depth=390.f+34.f*(Cottage%3);
   const float Y=CryptWorld::TownNorth-1080.f;
   const float Front=Y+Depth/2;
   Building(FVector(X,Y,Ground),375.f+43.f*Cottage,Depth,Cottage,false,180.f,Cottage==0);
   const float SquareEdge=CryptWorld::TownNorth-885.f;
   Box(Road,FVector(X,(Front+SquareEdge)*.5f,Ground+5),FVector(119,FMath::Abs(SquareEdge-Front)+20,10));
  }

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
   Box(Road, FVector(X+1100,0,Ground+4), FVector(340,HalfY*2,16));
   for (int32 Row = 0; Row < 4; ++Row)
   {
    const int32 Town = Column * 4 + Row;
    const float Y = (Row - 1.5f) * 2640;
    const float Street = Y - 480;
    SpawnMerchant(World,FVector(X+490,Street+118,Ground+16),Town);
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
    Box(Road, FVector(X, Street, Ground+6), FVector(2560,340,20));
    Box(Road, FVector(X+1100,Y+8,Ground+8), FVector(230,976,24));
    const float WorkDepth = 490 + 35 * (Town % 4);
    for (int32 Shop = 0; Shop < 2; ++Shop)
    {
     const float ShopY = Y + 260 + Shop * 720;
     Building(FVector(X+800, ShopY, Ground), 440 + 42*(Town % 3), WorkDepth, Town+Shop, true);
     Box(Road, FVector(X+940,ShopY-WorkDepth/2-65,Ground+10),FVector(440,138,28));
    }
    for (int32 Home = 0; Home < (Town < 10 ? 3 : 2); ++Home)
    {
     const float HomeX = X - 940 + 590*Home;
     const float Depth = 355 + 48*((Town+Home)%5);
     const float HomeY = Street+400+Depth/2;
     Building(FVector(HomeX, HomeY, Ground), 330 + 45*((Town+2*Home)%5), Depth, Town+Home, false);
     Box(Road, FVector(HomeX,Street+276,Ground+7),FVector(120,260,22));
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
      const bool bHalflingFamily = Town == 0 && Home == 0;
      const bool bWizard = !bHalflingFamily && Home == 0 && Member == 0 && Town % 2 == 0;
      Resident.Appearance = bHalflingFamily ? 4 : bWizard ? 5 : (Town+Home+Member%2)%4;
      Resident.Height = Member == 2 ? 117.f : 165.f + 7.f * ((Town+Member)%3);
      if (Resident.Appearance == 4) Resident.Height *= .67f;
      if (Resident.Appearance == 3) Resident.Height *= .80f;
      Resident.Shift = 3.f*Home + 2.f*Member + (Town%5);
      Resident.Route = Member < 2
       ? TArray<FVector>{AtHome,Door,Curb,Junction,ShopCorner,ShopDoor,Work}
       : TArray<FVector>{AtHome,Door,Curb,Curb+FVector(160,0,0),Door};
      Residents.Add(MoveTemp(Resident));
     }
    }
   }
  }

  const TCHAR* Cast[] = {TEXT("Villager"),TEXT("Artisan"),TEXT("Elf"),TEXT("Dwarf"),TEXT("Halfling"),TEXT("Wizard")};
  constexpr int32 Frames = 5;
  struct FAppearance
  {
   UInstancedStaticMeshComponent* Mesh[Frames]{};
   float Scale[Frames]{};
   float Feet[Frames]{};
  };
  TArray<FAppearance> Appearances;
  auto* Fallback = LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Characters/Town/Townsperson.Townsperson"));
  for (const TCHAR* Name : Cast)
  {
   auto& Appearance = Appearances.AddDefaulted_GetRef();
   for (int32 Frame=0;Frame<Frames;++Frame)
   {
    const FString Asset = FString::Printf(TEXT("%s%d"),Name,Frame);
    const FString Path = FString::Printf(TEXT("/Game/Characters/Town/Settlers/%s.%s"),*Asset,*Asset);
    auto* Mesh = LoadObject<UStaticMesh>(nullptr,*Path);
    if (!Mesh) Mesh = Fallback ? Fallback : Sphere;
    const auto Bounds = Mesh->GetBounds();
    Appearance.Mesh[Frame] = Batch(Mesh,nullptr,false,true);
    Appearance.Scale[Frame] = 1.f/FMath::Max(1.f,float(Bounds.BoxExtent.Z*2));
    Appearance.Feet[Frame] = Bounds.Origin.Z-Bounds.BoxExtent.Z;
   }
  }
  auto* Pets = Batch(Sphere, Oak, false, true);
  auto* Staff = Batch(Cube, Oak, false, true);
  auto* Lightning = Batch(Cube, LightningColor, false, true);
  auto* Flames = Batch(Sphere, FireColor, false, true);
  TArray<int32> LastFrames;
  LastFrames.Init(INDEX_NONE,Residents.Num());
  int32 Wizards = 0;
  const FTransform Hidden(FRotator::ZeroRotator,FVector::ZeroVector,FVector::ZeroVector);
  for (FResident& Resident : Residents)
  {
   if (Resident.bPet)
   {
    for (int32 Piece=0;Piece<7;++Piece) Pets->AddInstance(Hidden);
    continue;
   }
   for (auto* Frame : Appearances[Resident.Appearance].Mesh)
    Resident.MeshSlot = Frame->AddInstance(Hidden);
   if (Resident.Appearance == 5)
   {
    Resident.SpellSlot = Wizards++;
    Staff->AddInstance(Hidden);
    Flames->AddInstance(Hidden);
    for (int32 Piece=0;Piece<5;++Piece) Lightning->AddInstance(Hidden);
   }
  }
  TBitArray<> WasVisible(false, Residents.Num());
  auto Update = [World, Residents = MoveTemp(Residents), Appearances = MoveTemp(Appearances), Pets, Staff, Lightning, Flames,
                       LastFrames = MoveTemp(LastFrames), Hidden, WasVisible = MoveTemp(WasVisible)]() mutable
  {
   const APawn* Player = World->GetFirstPlayerController() ? World->GetFirstPlayerController()->GetPawn() : nullptr;
   const FVector Eye = Player ? Player->GetActorLocation() : FVector::ZeroVector;
   const float Time = World->GetTimeSeconds();
   int32 Pet = 0, ResidentIndex = 0;
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
    {
     Remaining -= FVector::Dist(Route[Segment-1],Route[Segment]);
     ++Segment;
    }
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
     const auto& Model = Appearances[Resident.Appearance];
     const int32 Frame = bVisible ? (bMoving ? 1+FMath::FloorToInt(Time*6+Resident.Shift)%4 : 0) : INDEX_NONE;
     const int32 Previous = LastFrames[Index];
     if (Previous != INDEX_NONE && Previous != Frame)
      Model.Mesh[Previous]->UpdateInstanceTransform(Resident.MeshSlot,Hidden,false,false,true);
     if (Frame != INDEX_NONE)
     {
      const float Scale = Resident.Height*Model.Scale[Frame];
      const float WorkRock = bWorking ? 4.f*FMath::Sin(Time*4+Resident.Shift) : 0.f;
      const FTransform Transform(FRotator(WorkRock,Facing.Yaw-90,0),
       Position+FVector(0,0,-Model.Feet[Frame]*Scale),FVector(Scale));
      Model.Mesh[Frame]->UpdateInstanceTransform(Resident.MeshSlot,Transform,false,false,true);
     }
     LastFrames[Index] = Frame;
     if (Resident.SpellSlot != INDEX_NONE)
     {
      const int32 Slot = Resident.SpellSlot;
      const float SpellTime = FMath::Fmod(Time+Resident.Shift,8.f);
      const bool bCasting = bVisible && SpellTime < 1.15f;
      const bool bFire = FMath::FloorToInt((Time+Resident.Shift)/8.f)%2 != 0;
      Pose(Staff,Slot,FVector(20,29,70),FVector(6,6,143),bCasting ? 42.f : 10.f);
      const FVector Start = Position+Facing.RotateVector(FVector(65,29,123));
      const FVector Direction = Facing.Vector();
      const FVector Right = Facing.RotateVector(FVector::RightVector);
      FVector From = Start;
      for (int32 Piece=0;Piece<5;++Piece)
      {
       const FVector To = Start+Direction*(48*(Piece+1))+Right*((Piece%2 ? -1 : 1)*22)
        +FVector(0,0,12*(Piece+1));
       const FVector Ray = To-From;
       const FTransform Bolt(Ray.Rotation(),(From+To)*.5,FVector(Ray.Size()/100.,.075,.075));
       Lightning->UpdateInstanceTransform(Slot*5+Piece,bCasting && !bFire && SpellTime < .42f ? Bolt : Hidden,false,false,true);
       From = To;
      }
      const FVector Fireball = Start+Direction*(SpellTime*310)+FVector(0,0,50*FMath::Sin(SpellTime*2.6f));
      Flames->UpdateInstanceTransform(Slot,bCasting && bFire
       ? FTransform(FRotator::ZeroRotator,Fireball,FVector(.54,.39,.42)) : Hidden,false,false,true);
     }
    }
   }
   if (bChanged)
   {
    for (const auto& Appearance : Appearances)
     for (auto* Mesh : Appearance.Mesh) Mesh->MarkRenderStateDirty();
    for (auto* Component : {Pets,Staff,Lightning,Flames}) Component->MarkRenderStateDirty();
   }
  };
  FTimerHandle Timer;
  World->GetTimerManager().SetTimer(Timer,FTimerDelegate::CreateWeakLambda(District, Update),.083f,true);
  Update();
 }
}
