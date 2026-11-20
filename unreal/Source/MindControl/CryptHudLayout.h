#pragma once

#include "CoreMinimal.h"

struct FCryptHudLayout
{
 FCryptHudLayout(float W,float H)
 {
  Scale=FMath::Max(.7f,FMath::Min(W/1600.f,H/900.f));
  Radius=FMath::Min(56.f*Scale,H*.075f);
  MapSize=FVector2D(FMath::Min(332.f*Scale,W*.32f),FMath::Min(214.f*Scale,H*.33f));
  MapOrigin=FVector2D(W-MapSize.X-18*Scale,12*Scale);
  const float Upper=MapOrigin.Y+MapSize.Y+Radius+8*Scale;
  const float Lower=Upper+2*Radius+8*Scale;
  const float Left=MapOrigin.X+MapSize.X*.25f;
  const float Right=MapOrigin.X+MapSize.X*.75f;
  Attack=FVector2D(Left,Upper);
  Jump=FVector2D(Right,Upper);
  Bag=FVector2D(Left,Lower);
  Spin=FVector2D(Right,Lower);
  Interact=FVector2D(110*Scale,FMath::Min(210*Scale,H*.47f));
 }

 bool Hit(const FVector2D& Point,const FVector2D& Center) const
 {
  return FMath::Abs(Point.X-Center.X)<=Radius && FMath::Abs(Point.Y-Center.Y)<=Radius;
 }

 float Scale;
 float Radius;
 FVector2D MapOrigin,MapSize,Attack,Jump,Bag,Spin,Interact;
};
