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
 double GetGroundHeight(double X,double Y);
 inline const FVector2D EntrancePoints[]={
  {-4900,1200},{-3100,2700},{-1200,4100},{1000,4200},{3200,3700},
  {3500,2200},{-4600,-1500},{-2200,-3900},{-900,-4200},{2800,-3600}
 };
 static_assert(UE_ARRAY_COUNT(EntrancePoints)==Count);
 inline constexpr double RampLength=1000.;
 inline constexpr double RampHalfWidth=200.;
 inline constexpr double LayoutScale=.5;
 inline constexpr double RoomHalfLength=687.5;
 inline constexpr double RoomHalfWidth=435.;

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
 inline FVector Room(int32 Index)
 {
  const auto& P=EntrancePoints[Index];
  const double X=P.X+RampLength+RoomHalfLength;
  double Ceiling=GetGroundHeight(P.X,P.Y)-115;
  for(double DX : {-RoomHalfLength,-RoomHalfLength/2,0.,RoomHalfLength/2,RoomHalfLength})
   for(double DY : {-RoomHalfWidth,0.,RoomHalfWidth})
    Ceiling=FMath::Min(Ceiling,GetGroundHeight(X+DX,P.Y+DY)-45);
  return FVector(X,P.Y,Ceiling-310);
 }
 inline bool IsRampCell(double X,double Y)
 {
  for(const auto& P : EntrancePoints)
   if(X>P.X && X<P.X+RampLength && FMath::Abs(Y-P.Y)<RampHalfWidth) return true;
  return false;
 }
 inline int32 Current(const FVector& Point)
 {
  for(int32 I=0;I<Count;++I)
  {
   const auto& P=EntrancePoints[I];
   if(Point.X<P.X+RampLength-20 || Point.X>P.X+RampLength+2*RoomHalfLength+20 ||
      FMath::Abs(Point.Y-P.Y)>RoomHalfWidth+20) continue;
   const double Height=Point.Z-Room(I).Z;
   if(Height>=-30 && Height<=283) return I;
  }
  return INDEX_NONE;
 }

 inline FVector MigrateLocation(FVector Position,bool bEnemy=false)
 {
  // Move saves made in the old separate rooms to the matching walk-in dungeon.
  if(FMath::Abs(Position.X-9200)<2140 && FMath::Abs(Position.Y)<1000)
   return FVector(-2450,210,GetGroundHeight(-2450,210)+115);
  for(int32 I=0;I<Count;++I)
  {
   const FVector OldCenter(14000+3600*(I%5),-4800+9600*(I/5),0);
   const FVector Offset=Position-OldCenter;
   if(FMath::Abs(Offset.X)<1400 && FMath::Abs(Offset.Y)<900)
    return bEnemy ? Room(I)+FVector(Offset.X*LayoutScale,Offset.Y*LayoutScale,95) : SurfaceArrival(I);
  }
  return Position;
 }
}
