#pragma once
#include "CoreMinimal.h"

namespace CryptDungeons
{
 enum class ELayout : uint8 { Pillars, Web, Forge, Marsh, Spires, Maze, Ice, Caldera, Cross, Vault };
 struct FChamber
 {
  const TCHAR* Name;
  ELayout Layout;
  int32 Enemies[4];
  FLinearColor Accent;
 };

 inline const FChamber Chambers[] = {
  {TEXT("Gravehollow Crypt"), ELayout::Pillars,{1,1,2,1},{.49f,.61f,.45f}},
  {TEXT("Widow's Warren"), ELayout::Web,{2,2,2,4},{.50f,.26f,.53f}},
  {TEXT("Cinder Forge"), ELayout::Forge,{3,3,6,8},{.85f,.24f,.08f}},
  {TEXT("Drowned Cistern"), ELayout::Marsh,{4,4,2,7},{.20f,.55f,.45f}},
  {TEXT("Stormglass Sanctum"), ELayout::Spires,{5,5,1,9},{.30f,.59f,.95f}},
  {TEXT("Iron Labyrinth"), ELayout::Maze,{6,2,6,6},{.53f,.54f,.61f}},
  {TEXT("Rimecatacombs"), ELayout::Ice,{7,7,5,9},{.50f,.77f,.91f}},
  {TEXT("Drakefire Caldera"), ELayout::Caldera,{8,3,3,8},{.84f,.37f,.08f}},
  {TEXT("Blackroot Shrine"), ELayout::Cross,{10,2,10,9},{.35f,.55f,.18f}},
  {TEXT("The Gilded Vault"), ELayout::Vault,{6,9,8,11},{.82f,.66f,.26f}}
 };
 inline constexpr int32 Count=UE_ARRAY_COUNT(Chambers);
 inline constexpr int32 EnemyId(int32 Chamber,int32 Slot) { return 1000+Chamber*10+Slot; }
 inline FVector SurfaceGate() { return FVector(-2450,440,-20); }
 inline constexpr float FloorElevation=240.f;
 double GetGroundHeight(double X,double Y);
 inline const FVector2D EntrancePoints[]={
  {-4950,1750},{-3150,2700},{-1200,4100},{950,4250},{3170,3670},
  {4870,2350},{-4650,-1450},{-2450,-2950},{-940,-4250},{2800,-3650}
 };
 static_assert(UE_ARRAY_COUNT(EntrancePoints)==Count);

 inline FVector Entrance(int32 Index)
 {
  const auto& P=EntrancePoints[Index];
  return FVector(P.X,P.Y,GetGroundHeight(P.X,P.Y)+4);
 }
 inline FVector SurfaceArrival(int32 Index)
 {
  const auto& P=EntrancePoints[Index];
  return FVector(P.X-285,P.Y,GetGroundHeight(P.X-285,P.Y)+115);
 }
 inline FVector Hub() { return FVector(9200,0,FloorElevation); }
 inline FVector Room(int32 Index) { return FVector(14000+3600*(Index%5),-4800+9600*(Index/5),FloorElevation); }
 inline FVector Gate(int32 Index) { return Hub()+FVector(-1440+720*(Index%5),Index<5?-640:640,0); }
 inline FVector Exit(int32 Index) { return Room(Index)+FVector(-1050,0,0); }
 inline bool Near(const FVector& A,const FVector& B) { return FVector::DistSquared(A,B)<FMath::Square(250.); }
 inline int32 Current(const FVector& Point)
 {
  for(int32 I=0;I<Count;++I)
   if(FVector::DistSquared(Point,Room(I))<FMath::Square(1740.)) return I;
  return INDEX_NONE;
 }
 inline FVector RaiseOldSave(FVector Position)
 {
  // Previous builds placed these floors at Z=0.
  if(Position.Z>=FloorElevation-5) return Position;
  if(FMath::Abs(Position.X-Hub().X)<2140 && FMath::Abs(Position.Y)<1000)
   Position.Z+=FloorElevation;
  else for(int32 I=0;I<Count;++I)
  {
   const FVector Offset=Position-Room(I);
   if(FMath::Abs(Offset.X)<1380 && FMath::Abs(Offset.Y)<870)
   {
    Position.Z+=FloorElevation;
    break;
   }
  }
  return Position;
 }
 inline bool Travel(const FVector& Point,FVector& Destination,FString& Label)
 {
  if(Near(Point,SurfaceGate()))
  {
   Destination=Hub()+FVector(-1700,0,100);
   Label=TEXT("Enter dungeon crossroads");
   return true;
  }
  if(Near(Point,Hub()+FVector(-1910,0,0)))
  {
   Destination=SurfaceGate()+FVector(0,-220,110);
   Label=TEXT("Return to the surface");
   return true;
  }
  for(int32 I=0;I<Count;++I)
  {
   if(Near(Point,Gate(I)) || Near(Point,Entrance(I)))
   {
    Destination=Room(I)+FVector(-780,0,100);
    Label=FString::Printf(TEXT("Enter %s"),Chambers[I].Name);
    return true;
   }
   if(Near(Point,Exit(I)))
   {
    Destination=SurfaceArrival(I);
    Label=TEXT("Return to the surface");
    return true;
   }
  }
  return false;
 }
}
