#pragma once

#include "CoreMinimal.h"

namespace CryptWorld
{
 inline constexpr float West=-6000.f;
 inline constexpr float East=6000.f;
 inline constexpr float South=-5500.f;
 inline constexpr float North=5500.f;
 inline const FVector2D Castle(4540,0);
 inline const FVector2D Town(2420,0);

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

 inline FVector2D MapPoint(const FVector& Location)
 {
  return FVector2D(FMath::Clamp((Location.X-West)/(East-West),0.,1.),
    FMath::Clamp((North-Location.Y)/(North-South),0.,1.));
 }
}
