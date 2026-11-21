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
 inline const FVector2D LegacyEntrancePoints[]={
  {-4900,1200},{-3100,2700},{-1200,4100},{1000,2900},{3200,3700},
  {3500,2200},{-4600,-1500},{-2800,-3900},{-300,-4200},{2800,-3600}
 };
 inline const FVector2D ExpandedWorldEntrancePoints[]={
  {-4900,1200},{-12600,14800},{-4900,26000},{14300,20400},{31600,11200},
  {3500,2200},{-16200,-12900},{-27400,-27600},{-300,-4200},{28800,-22800}
 };
 inline const FVector2D EntrancePoints[]={
  {-4900,1200},{-12600,14800},{-4900,34000},{14300,20400},{31600,10500},
  {25000,-4000},{-41000,-12900},{-27400,-27600},{-300,-10500},{28800,-22800}
 };
 static_assert(UE_ARRAY_COUNT(EntrancePoints)==Count);
 inline constexpr double Enlargement=7.071067811865475244; // sqrt(50) on each axis
 inline constexpr double CeilingHeight=620.;
 inline constexpr double RampLength=4000.;
 inline constexpr double RampHalfWidth=200.;
 inline constexpr double LayoutScale=.72*Enlargement;
 inline constexpr double RoomHalfLength=1300.*Enlargement;
 inline constexpr double RoomHalfWidth=660.*Enlargement;

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
  static const TArray<FVector> Rooms=[]
  {
   TArray<FVector> Result;
   Result.Reserve(Count);
   for(const auto& P : EntrancePoints)
   {
    const double X=P.X+RampLength+RoomHalfLength;
    double Roof=GetGroundHeight(P.X,P.Y)-160.;
    for(int32 Row=0;Row<=12;++Row)
     for(int32 Column=0;Column<=24;++Column)
     {
      const double SampleX=X+RoomHalfLength*(Column/12.-1.);
      const double SampleY=P.Y+RoomHalfWidth*(Row/6.-1.);
      Roof=FMath::Min(Roof,GetGroundHeight(SampleX,SampleY)-280.);
     }
    Result.Emplace(X,P.Y,Roof-CeilingHeight);
   }
   return Result;
  }();
  return Rooms[Index];
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
   if(Height>=-30 && Height<=CeilingHeight-10) return I;
  }
  return INDEX_NONE;
 }

 inline FVector MigrateLocation(FVector Position,int32 Version)
 {
  if(Version==3)
  {
   for(int32 I=0;I<Count;++I)
   {
    const auto& P=ExpandedWorldEntrancePoints[I];
    const double Along=Position.X-P.X;
    const double Across=FMath::Abs(Position.Y-P.Y);
    const double OldHalfWidth=Along<1000.?200.:660.;
    if(Along>=0 && Along<=3616. && Across<=OldHalfWidth+12 &&
       Position.Z<GetGroundHeight(Position.X,Position.Y)-100.)
     return SurfaceArrival(I);
   }
   return Position;
  }
  // Move older saves from the old separate rooms to the matching walk-in dungeon.
  if(FMath::Abs(Position.X-9200)<2140 && FMath::Abs(Position.Y)<1000)
   return FVector(-2450,210,GetGroundHeight(-2450,210)+115);
  for(int32 I=0;I<Count;++I)
  {
   const auto& Legacy=LegacyEntrancePoints[I];
   const bool bLegacyRoom=Position.X>=Legacy.X+980 && Position.X<=Legacy.X+2395 &&
       FMath::Abs(Position.Y-Legacy.Y)<=455 &&
       Position.Z<GetGroundHeight(Position.X,Position.Y)-140;
   if(bLegacyRoom) return SurfaceArrival(I);

   const FVector OldCenter(14000+3600*(I%5),-4800+9600*(I/5),0);
   const FVector Offset=Position-OldCenter;
   if(FMath::Abs(Offset.X)<1400 && FMath::Abs(Offset.Y)<900)
    return SurfaceArrival(I);
  }
  return Position;
 }
}
