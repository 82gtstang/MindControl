#pragma once

#include "CoreMinimal.h"

namespace CryptWorld
{
 inline constexpr float HalfWorldX=66000.f;
 inline constexpr float HalfWorldY=60000.f;
 inline constexpr float West=-6000.f;
 inline constexpr float CastleOffsetX=7000.f;
 inline constexpr float MainRoadEnd=12300.f;
 inline constexpr float East=13200.f;
 inline constexpr float South=-5500.f;
 inline constexpr float North=5500.f;
 inline const FVector2D Castle(4540+CastleOffsetX,0);
 inline constexpr float TownNorth=4800.f;
 inline constexpr float TownRoadX=3650.f;
 inline constexpr float RuinOffsetX=5500.f;
 inline const FVector2D Town(2420,TownNorth);

 // The pool sits on the north bank of the Gravehollow entrance trench.
 inline constexpr float PoolX=-1950.f, PoolY=2200.f;
 inline constexpr float PoolRadiusX=950.f, PoolRadiusY=740.f;
 inline constexpr float PoolSurface=-90.f;
 inline double PoolRadius(double X,double Y)
 {
  return FMath::Sqrt(FMath::Square((X-PoolX)/PoolRadiusX)+FMath::Square((Y-PoolY)/PoolRadiusY));
 }

 inline constexpr float OceanLimit=108000.f;
 inline constexpr float SeaLevel=-550.f;
 struct FIsland
 {
  const TCHAR* Name;
  FVector2D Center;
  float Radius;
 };
 inline const FIsland Islands[]={
  {TEXT("Gull Cay"),{85000,21000},5600},
  {TEXT("Lantern Isle"),{42000,82000},6500},
  {TEXT("Saltwind Key"),{-14000,86500},5500},
  {TEXT("Black Sail Isle"),{-82000,47000},6900},
  {TEXT("Driftwood Cay"),{-87500,-21500},4800},
  {TEXT("Pearl Shoal"),{-38000,-83500},5800},
  {TEXT("Stormhook Isle"),{50000,-81500},6900}
 };
 inline double MainlandEdge(double X,double Y)
 {
  return FMath::Max(FMath::Abs(X)/HalfWorldX,FMath::Abs(Y)/HalfWorldY);
 }
 inline FVector DockDirection(int32 Index)
 {
  if(Index==0) return FVector::ForwardVector;
  const auto& P=Islands[Index-1].Center;
  return FVector(P.X,P.Y,0).GetSafeNormal();
 }
 inline FVector DockEnd(int32 Index)
 {
  if(Index==0) return FVector(HalfWorldX,0,SeaLevel+90.f);
  const auto& Island=Islands[Index-1];
  return FVector(Island.Center.X,Island.Center.Y,SeaLevel+90.f)+DockDirection(Index)*Island.Radius;
 }
 inline FVector ShipBerth(int32 Index)
 {
  FVector Berth=DockEnd(Index)+DockDirection(Index)*1050.;
  Berth.Z=SeaLevel;
  return Berth;
 }

 struct FRiver
 {
  float Start,End,Center,Width,Phase;
  bool bFalls;

  float BedWidth() const { return Width + 125.f; }

  float X(float Y) const { return Center+220.f*FMath::Sin(Y/590.f+Phase); }
  float Height(float Y) const
  {
   const float T=FMath::Clamp((Y-Start)/(End-Start),0.f,1.f);
   if(bFalls)
    return -88.f+80.f*T+320.f*FMath::SmoothStep(.47f,.53f,T);
   return -155.f+125.f*T;
  }
 };
 inline constexpr FRiver Rivers[]={
  {1600.f,5200.f,-4180.f,195.f,0.f,true},
  {-5200.f,-1350.f,900.f,180.f,1.1f,false},
  {-5200.f,-1300.f,4710.f,175.f,2.4f,false}
 };

 inline FVector2D MapPoint(const FVector& Location,bool bOverview=false)
 {
  if(bOverview)
   return FVector2D(FMath::Clamp((Location.X+OceanLimit)/(2*OceanLimit),0.,1.),
     FMath::Clamp((OceanLimit-Location.Y)/(2*OceanLimit),0.,1.));
  return FVector2D(FMath::Clamp((Location.X-West)/(East-West),0.,1.),
    FMath::Clamp((North-Location.Y)/(North-South),0.,1.));
 }
}
