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
   return FVector2D(FMath::Clamp((Location.X+HalfWorldX)/(2*HalfWorldX),0.,1.),
     FMath::Clamp((HalfWorldY-Location.Y)/(2*HalfWorldY),0.,1.));
  return FVector2D(FMath::Clamp((Location.X-West)/(East-West),0.,1.),
    FMath::Clamp((North-Location.Y)/(North-South),0.,1.));
 }
}
