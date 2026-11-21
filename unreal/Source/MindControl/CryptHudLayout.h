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
  Fire=FVector2D(MapOrigin.X-Radius-12*Scale,Upper);
  Storm=FVector2D(MapOrigin.X-3*Radius-22*Scale,Upper);
  Attack=FVector2D(Left,Upper);
  Jump=FVector2D(Right,Upper);
  Bag=FVector2D(Left,Lower);
  Spin=FVector2D(Right,Lower);
  Interact=FVector2D(110*Scale,FMath::Min(210*Scale,H*.47f));
  SatchelSize=FVector2D(FMath::Min(W*.58f,H*1.03f),H*.88f);
  SatchelOrigin=FVector2D(W*.433f-SatchelSize.X*.5f,H*.025f);
  ItemRadius=FMath::Min(SatchelSize.X*.101f,SatchelSize.Y*.077f);
  UtilityRadius=FMath::Min(ItemRadius*.76f,32.f*Scale);
  Save=InSatchel(.18f,.95f);
  Restore=InSatchel(.82f,.95f);
 }

 bool Hit(const FVector2D& Point,const FVector2D& Center) const
 {
  return FMath::Abs(Point.X-Center.X)<=Radius && FMath::Abs(Point.Y-Center.Y)<=Radius;
 }

 FVector2D InSatchel(float X,float Y) const
 {
  return SatchelOrigin+FVector2D(X*SatchelSize.X,Y*SatchelSize.Y);
 }

 FVector2D WeaponPocket(int32 Index) const
 {
  return InSatchel(.245f+.17f*Index,.44f);
 }

 FVector2D SupplyPocket(int32 Index) const
 {
  return InSatchel(.215f+.1425f*Index,.61f);
 }

 bool HitPocket(const FVector2D& Point,const FVector2D& Center,float HalfSize) const
 {
  return FMath::Abs(Point.X-Center.X)<=HalfSize && FMath::Abs(Point.Y-Center.Y)<=HalfSize;
 }

 FVector2D SatchelOrigin,SatchelSize,Save,Restore;
 float ItemRadius,UtilityRadius;
 float Scale;
 float Radius;
 FVector2D MapOrigin,MapSize,Attack,Jump,Bag,Spin,Interact,Fire,Storm;
};
