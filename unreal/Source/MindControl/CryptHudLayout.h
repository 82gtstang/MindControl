#pragma once

#include "CoreMinimal.h"

struct FCryptHudLayout
{
 static FVector2D LeftStickAnchor() { return FVector2D(.125f,.855f); }
 static FVector2D RightStickAnchor() { return FVector2D(.875f,.855f); }
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
  SatchelSize=FVector2D(FMath::Min(W*.58f,H*1.03f),H*.88f);
  SatchelOrigin=FVector2D(W*.433f-SatchelSize.X*.5f,H*.025f);
  ItemRadius=FMath::Min(SatchelSize.X*.101f,SatchelSize.Y*.077f);
  UtilityRadius=FMath::Min(ItemRadius*.76f,32.f*Scale);
  Save=InSatchel(.18f,.95f);
  Restore=InSatchel(.82f,.95f);
  RightThumbCenter=RightStickAnchor()*FVector2D(W,H);
  RightThumbRadius=66.f*Scale;
  LogSize=FVector2D(FMath::Min(W*.235f,255.f*Scale),105.f*Scale);
  LogOrigin=FVector2D(20.f*Scale,H*.62f-LogSize.Y);
  LogOlder=LogOrigin+FVector2D(LogSize.X-46.f*Scale,13.f*Scale);
  LogNewer=LogOrigin+FVector2D(LogSize.X-18.f*Scale,13.f*Scale);
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

 FVector2D ThumbActionOption(int32 Index) const
 {
  return InSatchel(.18f+.16f*Index,.80f);
 }

 bool HitThumbAction(const FVector2D& Point,int32 Index) const
 {
  const FVector2D Delta=Point-ThumbActionOption(Index);
  return FMath::Abs(Delta.X)<=SatchelSize.X*.075 &&
   FMath::Abs(Delta.Y)<=SatchelSize.Y*.040;
 }

 FVector2D LogSize,LogOrigin,LogOlder,LogNewer;
 FVector2D RightThumbCenter;
 float RightThumbRadius;

 FVector2D GearButton(int32 Index) const { return InSatchel(.19f+.20f*Index,.70f); }
 FVector2D GearTab() const { return InSatchel(.72f,.27f); }
 bool HitGear(const FVector2D& Point,const FVector2D& Center) const
 {
  return FMath::Abs(Point.X-Center.X)<=SatchelSize.X*.085 &&
         FMath::Abs(Point.Y-Center.Y)<=SatchelSize.Y*.045;
 }

 bool HitPocket(const FVector2D& Point,const FVector2D& Center,float HalfSize) const
 {
  return FMath::Abs(Point.X-Center.X)<=HalfSize && FMath::Abs(Point.Y-Center.Y)<=HalfSize;
 }

 FVector2D SatchelOrigin,SatchelSize,Save,Restore;
 float ItemRadius,UtilityRadius;
 float Scale;
 float Radius;
 FVector2D MapOrigin,MapSize,Attack,Jump,Bag,Spin,Interact;
};
