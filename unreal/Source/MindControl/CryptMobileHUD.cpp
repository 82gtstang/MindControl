#include "CryptMobileHUD.h"
#include "CryptHero.h"
#include "CryptMonster.h"
#include "CryptDungeons.h"
#include "CryptHudLayout.h"
#include "CryptWorldLayout.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include <initializer_list>

void ACryptMobileHUD::DrawHUD()
{
 Super::DrawHUD();
 if(!Canvas) return;
 const float W=Canvas->SizeX, H=Canvas->SizeY;
 const FCryptHudLayout Layout(W,H);
 const float S=Layout.Scale;
 const FLinearColor Panel(.035f,.055f,.08f,.9f);
 const FLinearColor Gold(.96f,.79f,.44f);
 const FLinearColor Bronze(.53f,.37f,.21f,.95f);
 const FLinearColor Ink(.061f,.083f,.099f,.97f);
 enum class EIcon { Fist, Sword, Axe, Spear, Jump, Spin, Bag, Talk, Save, Restore, Castle, Town, Fire, Storm, Bow };
 const auto Stroke=[&](FVector2D A,FVector2D B,FLinearColor Color,float Width=1.6f)
 {
  DrawLine(A.X,A.Y,B.X,B.Y,Color,Width*S);
 };
 const auto Frame=[&](float X,float Y,float Width,float Height,FLinearColor Fill,FLinearColor Border)
 {
  const float T=FMath::Max(1.f,1.4f*S);
  DrawRect(FLinearColor(.005f,.009f,.012f,.62f),X+3*S,Y+4*S,Width,Height);
  DrawRect(Border,X,Y,Width,Height);
  DrawRect(Fill,X+T,Y+T,Width-2*T,Height-2*T);
  DrawRect(FLinearColor(.86f,.67f,.38f,.70f),X+T,Y+T,FMath::Min(Width-2*T,18.f*S),T);
  DrawRect(FLinearColor(.86f,.67f,.38f,.70f),X+Width-19.4f*S,Y+Height-2*T,18*S,T);
 };
 const auto Icon=[&](EIcon Shape,FVector2D Center,float Size,FLinearColor Color)
 {
  const auto Segment=[&](float X1,float Y1,float X2,float Y2)
  {
   Stroke(Center+FVector2D(X1,Y1)*Size,Center+FVector2D(X2,Y2)*Size,Color,1.85f);
  };
  const auto Path=[&](std::initializer_list<FVector2D> Points)
  {
   auto Previous=Points.begin();
   for(auto Next=Previous+1;Next!=Points.end();++Next,++Previous)
    Stroke(Center+*Previous*Size,Center+*Next*Size,Color,1.8f);
  };
  switch(Shape)
  {
   case EIcon::Bow:
    Path({{-.38,-.83},{.14,-.66},{.45,-.25},{.52,0},{.45,.25},{.14,.66},{-.38,.83},{-.38,-.83}});
    Path({{-.75,0},{.80,0},{.54,-.24},{.80,0},{.54,.24}});
    break;
   case EIcon::Fire:
    Path({{-.32,.87},{-.76,.38},{-.62,-.18},{-.19,.12},{.11,-.91},{.67,-.19},{.78,.49},{.38,.87},{-.32,.87}});
    break;
   case EIcon::Storm:
    Path({{.16,-.91},{-.67,.14},{-.09,.14},{-.39,.92},{.72,-.23},{.15,-.23},{.16,-.91}});
    break;
   case EIcon::Fist:
    Path({{-.7,.65},{-.86,-.1},{-.55,-.69},{.48,-.78},{.81,-.4},{.72,.2},{.39,.65},{-.7,.65}});
    Path({{-.58,.2},{-.23,-.16},{.38,-.07},{.65,.16}});
    for(int32 Knuckle=0;Knuckle<3;++Knuckle) Segment(-.45f+.32f*Knuckle,-.68f,-.35f+.32f*Knuckle,-.28f);
    break;
   case EIcon::Sword:
    Path({{-.25,.06},{.71,-.86},{.88,-.78},{.12,.36}});
    Segment(-.43,.06,.22,.57); Segment(-.08,.31,-.61,.86);
    break;
   case EIcon::Axe:
    Segment(-.61,.84,.45,-.74);
    Path({{.25,-.44},{-.15,-.81},{.45,-.87},{.89,-.48},{.67,.06},{.25,-.44}});
    break;
   case EIcon::Spear:
    Segment(-.69,.86,.51,-.46);
    Path({{.35,-.31},{.38,-.69},{.86,-.87},{.75,-.35},{.35,-.31}});
    break;
   case EIcon::Jump:
    Path({{-.63,-.17},{0,-.79},{.63,-.17}});
    Segment(0,-.76,0,.57); Segment(-.55,.70,.55,.70);
    break;
   case EIcon::Spin:
    Path({{.72,-.54},{.28,-.83},{-.33,-.79},{-.77,-.39},{-.80,.25},{-.42,.71},{.17,.82},{.73,.44}});
    Path({{.78,-.92},{.77,-.51},{.34,-.46}});
    break;
   case EIcon::Bag:
    Path({{-.63,-.29},{-.49,-.01},{-.83,.37},{-.66,.77},{.66,.77},{.83,.37},{.48,-.01},{.63,-.29},{-.63,-.29}});
    Path({{-.55,-.34},{-.67,-.72},{.66,-.72},{.55,-.34}}); Segment(-.43,-.08,.43,-.08);
    break;
   case EIcon::Talk:
    Path({{-.8,-.62},{.8,-.62},{.8,.37},{.06,.37},{-.48,.78},{-.44,.37},{-.8,.37},{-.8,-.62}});
    Segment(-.53,-.23,.54,-.23); Segment(-.53,.03,.30,.03);
    break;
   case EIcon::Save:
    Path({{-.73,-.62},{.52,-.62},{.74,-.4},{.74,.68},{-.73,.68},{-.73,-.62}});
    Path({{-.42,-.62},{-.42,-.12},{.36,-.12},{.36,-.62}}); Segment(-.29,.36,.29,.36);
    break;
   case EIcon::Restore:
    Path({{.72,-.45},{.24,-.78},{-.42,-.72},{-.72,-.25},{-.66,.33},{-.19,.74},{.40,.67},{.76,.26}});
    Path({{-.77,-.70},{-.73,-.24},{-.29,-.29}});
    break;
   case EIcon::Castle:
    Path({{-.84,.69},{-.84,-.58},{-.51,-.58},{-.51,-.23},{-.16,-.23},{-.16,-.58},{.17,-.58},{.17,-.23},{.51,-.23},{.51,-.58},{.84,-.58},{.84,.69},{-.84,.69}});
    Path({{-.25,.66},{-.25,.19},{.25,.19},{.25,.66}});
    break;
   case EIcon::Town:
    Path({{-.82,-.12},{0,-.78},{.82,-.12},{.54,-.12},{.54,.69},{-.54,.69},{-.54,-.12},{-.82,-.12}});
    Path({{-.18,.69},{-.18,.19},{.19,.19},{.19,.69}});
    break;
  }
 };
 const auto WeaponIcon=[](ECryptWeapon Weapon)
 {
  switch(Weapon)
  {
   case ECryptWeapon::Sword:return EIcon::Sword;
   case ECryptWeapon::Axe:return EIcon::Axe;
   case ECryptWeapon::Spear:return EIcon::Spear;
   case ECryptWeapon::Bow:return EIcon::Bow;
   default:return EIcon::Fist;
  }
 };
 const auto WeaponName=[](ECryptWeapon Weapon)
 {
  switch(Weapon)
  {
   case ECryptWeapon::Sword:return TEXT("Longsword");
   case ECryptWeapon::Axe:return TEXT("Battle axe");
   case ECryptWeapon::Spear:return TEXT("Spear");
   case ECryptWeapon::Bow:return TEXT("Bow");
   default:return TEXT("Unarmed");
  }
 };
 const ACryptHero* Hero=Cast<ACryptHero>(GetOwningPawn());
 if(!Hero) return;

 const auto& Trail=Hero->GetStrikeTrail();
 const float Strength=Hero->GetStrikeStrength();
 const bool bArmed=Hero->EquippedWeapon!=ECryptWeapon::Unarmed;
 const FLinearColor Glow=bArmed?FLinearColor(.78f,.91f,1.f):FLinearColor(1.f,.70f,.38f);
 if(const APlayerController* Controller=GetOwningPlayerController())
 {
  for(int32 Index=1;Index<Trail.Num();++Index)
  {
   FVector2D A,B;
   if(!Controller->ProjectWorldLocationToScreen(Trail[Index-1],A) || !Controller->ProjectWorldLocationToScreen(Trail[Index],B)) continue;
   const float Opacity=Strength*float(Index)/Trail.Num();
   Stroke(A,B,Glow.CopyWithNewOpacity(.30f*Opacity),bArmed?8.f:4.f);
   Stroke(A,B,Glow.CopyWithNewOpacity(.90f*Opacity),bArmed?2.8f:1.8f);
  }
 }
 if(!bSatchelTextureChecked)
 {
  bSatchelTextureChecked=true;
  SatchelTexture=LoadObject<UTexture2D>(nullptr,TEXT("/Game/Environment/MindControl/T_Satchel.T_Satchel"));
 }
 const auto Sprite=[&](int32 Cell,FVector2D Center,FVector2D Size,float Opacity=1.f)
 {
  if(!SatchelTexture || Opacity<=0.f) return;
  const double U=(Cell%4)*.25;
  const double V=(Cell/4)/3.0;
  DrawTexture(SatchelTexture,Center.X-Size.X*.5,Center.Y-Size.Y*.5,Size.X,Size.Y,
   U,V,.25,1.0/3.0,FLinearColor(1,1,1,Opacity),BLEND_Translucent);
 };
 const auto Button=[&](const FVector2D& Center,EIcon Shape,const TCHAR* Label,bool bSelected=false)
 {
  const float R=Layout.Radius;
  Frame(Center.X-R,Center.Y-R,2*R,2*R,bSelected?FLinearColor(.24f,.16f,.085f,.96f):Ink,bSelected?Gold:Bronze);
  if(Shape==EIcon::Bag && SatchelTexture)
   Sprite(bSelected?1:0,Center-FVector2D(0,R*.18f),FVector2D(R*1.08f,R*1.08f));
  else Icon(Shape,Center-FVector2D(0,R*.17f),R*.33f,Gold);
  float Width=0,Height=0;
  GetTextSize(Label,Width,Height,nullptr,.90f*S);
  DrawText(Label,FLinearColor::White,Center.X-Width*.5,Center.Y+R*.43f-Height*.5,nullptr,.90f*S,false);
 };
 Button(Layout.Jump,EIcon::Jump,TEXT("JUMP"));
 const bool bBow=Hero->EquippedWeapon==ECryptWeapon::Bow;
 if(!bBow) Button(Layout.Attack,WeaponIcon(Hero->EquippedWeapon),bArmed?TEXT("ATTACK"):TEXT("PUNCH"),Strength>.1f);
 Button(Layout.Spin,EIcon::Spin,TEXT("SPIN"));
 if(!Hero->bInventoryOpen)
 {
  const float BarWidth=FMath::Min(200.f*S,W*.18f);
  const float BarX=W*.40f;
  const auto Resource=[&](const TCHAR* Label,float Current,float Maximum,float Y,FLinearColor Color)
  {
   DrawRect(Ink,BarX,Y,BarWidth,16*S);
   DrawRect(Color,BarX+S,Y+S,(BarWidth-2*S)*FMath::Clamp(Current/Maximum,0.f,1.f),14*S);
   DrawText(FString::Printf(TEXT("%s %d/%d"),Label,FMath::CeilToInt(Current),int32(Maximum)),
    FLinearColor::White,BarX+5*S,Y,nullptr,.80f*S,false);
  };
  Resource(TEXT("HP"),Hero->GetHealth(),100.f,12*S,FLinearColor(.61f,.15f,.14f));
  Resource(TEXT("STA"),Hero->GetStamina(),100.f,30*S,FLinearColor(.25f,.54f,.20f));
  Resource(TEXT("MP"),Hero->GetMana(),75.f,48*S,FLinearColor(.15f,.34f,.67f));
  DrawText(FString::Printf(TEXT("DEX %d   ARM %d   WPN %d"),int32(Hero->GetDexterity()),
    int32(Hero->GetArmorRating()),int32(Hero->WeaponDamage())),
    Gold,BarX,67*S,nullptr,.92f*S,false);
  TArray<FString> Encounter;
  Hero->GetCombatTarget().ParseIntoArrayLines(Encounter);
  for(int32 I=0;I<Encounter.Num();++I)
   DrawText(Encounter[I],FLinearColor::White,BarX,(86+17*I)*S,nullptr,.92f*S,false);
 }
 Button(Layout.Bag,EIcon::Bag,TEXT("BAG"),Hero->bInventoryOpen);
 if(!Hero->bInventoryOpen)
 {
  const EIcon ThumbIcons[]={EIcon::Jump,WeaponIcon(Hero->EquippedWeapon),EIcon::Spin,EIcon::Fire,EIcon::Storm};
  Icon(bBow?EIcon::Bow:ThumbIcons[int32(Hero->GetThumbAction())],Layout.RightThumbCenter,17*S,Gold);
  if(bBow)
  {
   const TCHAR* BowHint=Hero->IsBowAiming()?TEXT("RELEASE TO SHOOT"):TEXT("HOLD TO AIM");
   float HintWidth=0,HintHeight=0;
   GetTextSize(BowHint,HintWidth,HintHeight,nullptr,.76f*S);
   DrawText(BowHint,Gold,Layout.RightThumbCenter.X-HintWidth*.5f,
    Layout.RightThumbCenter.Y-Layout.RightThumbRadius-HintHeight-4*S,nullptr,.76f*S,false);
  }
 }


 if(Hero->IsBowAiming())
 {
  const FVector2D Center(W*.5f,H*.5f);
  const FLinearColor Reticle(.96f,.95f,.87f);
  for(const FVector2D Offset : {FVector2D(1,0),FVector2D(-1,0),FVector2D(0,1),FVector2D(0,-1)})
   Stroke(Center+Offset*5*S,Center+Offset*15*S,Reticle,1.6f);
  DrawRect(Reticle,Center.X-S,Center.Y-S,2*S,2*S);
  return;
 }

 const FVector2D Origin=Layout.MapOrigin,Size=Layout.MapSize;
 const int32 Chart=Hero->GetChartIndex();
 if(Chart!=INDEX_NONE)
 {
  Frame(Origin.X,Origin.Y,Size.X,Size.Y,FLinearColor(.029f,.027f,.024f,.98f),Bronze);
  DrawText(CryptDungeons::Chambers[Chart].Name,Gold,Origin.X+9*S,Origin.Y+5*S,nullptr,.82f*S,false);
  const FVector2D Cell((Size.X-16*S)/CryptDungeons::ChartColumns,
                        (Size.Y-30*S)/CryptDungeons::ChartRows);
  const FVector2D Grid=Origin+FVector2D(8*S,23*S);
  constexpr int32 Tiles=CryptDungeons::ChartColumns*CryptDungeons::ChartRows;
  for(int32 Key : Hero->GetChartTiles())
  {
   if(Key<Chart*Tiles || Key>=(Chart+1)*Tiles) continue;
   const int32 Tile=Key%Tiles;
   DrawRect(FLinearColor(.49f,.42f,.31f),Grid.X+(Tile%CryptDungeons::ChartColumns)*Cell.X,
    Grid.Y+(Tile/CryptDungeons::ChartColumns)*Cell.Y,Cell.X,Cell.Y);
  }
  const int32 Tile=CryptDungeons::ChartTile(Chart,Hero->GetActorLocation());
  if(Tile!=INDEX_NONE)
  {
   const FVector2D Point=Grid+FVector2D((Tile%CryptDungeons::ChartColumns+.5)*Cell.X,
     (Tile/CryptDungeons::ChartColumns+.5)*Cell.Y);
   const FVector Forward=Hero->GetActorForwardVector();
   const FVector2D Direction=FVector2D(Forward.X,-Forward.Y).GetSafeNormal();
   const FVector2D Across(-Direction.Y,Direction.X);
   Stroke(Point+Direction*5*S,Point-Direction*3*S+Across*3*S,FLinearColor::White);
   Stroke(Point+Direction*5*S,Point-Direction*3*S-Across*3*S,FLinearColor::White);
  }
 }
 else
 {
 const FLinearColor RiverColor(.31f,.72f,.88f);
 Frame(Origin.X,Origin.Y,Size.X,Size.Y,FLinearColor(.068f,.115f,.080f,.97f),Bronze);
 const FVector HeroPosition=Hero->GetActorLocation();
 const bool bOverview=HeroPosition.X<CryptWorld::West || HeroPosition.X>CryptWorld::East ||
  FMath::Abs(HeroPosition.Y)>CryptWorld::North;
 if(bOverview)
  DrawText(TEXT("WORLD"),Gold,Origin.X+17*S,Origin.Y+4*S,nullptr,.83f*S,false);
 const auto ToMap=[&](const FVector& Location)
 {
  const FVector2D Unit=CryptWorld::MapPoint(Location,bOverview);
  return Origin+FVector2D(8*S+Unit.X*(Size.X-16*S),8*S+Unit.Y*(Size.Y-16*S));
 };
 const auto MapLine=[&](const FVector2D& A,const FVector2D& B,const FLinearColor& Color,float Thickness)
 {
  DrawLine(A.X,A.Y,B.X,B.Y,Color,Thickness*S);
 };
 for(const FVector2D Hill : {FVector2D(-3250,-2800),FVector2D(3400,-2950),FVector2D(-3300,3200),FVector2D(4330,3200)})
 {
  for(int32 Ring=1;Ring<=3;++Ring)
  {
   const float Radius=Ring*390.f;
   FVector2D Prior=ToMap(FVector(Hill.X+Radius,Hill.Y,0));
   for(int32 Step=1;Step<=20;++Step)
   {
    const float Angle=2*PI*Step/20.f;
    const FVector2D Point=ToMap(FVector(Hill.X+Radius*FMath::Cos(Angle),Hill.Y+Radius*.71f*FMath::Sin(Angle),0));
    MapLine(Prior,Point,FLinearColor(.22f,.33f,.18f,.72f),.8f);
    Prior=Point;
   }
  }
 }
 MapLine(ToMap(FVector(-5500,0,0)),ToMap(FVector(CryptWorld::MainRoadEnd,0,0)),FLinearColor(.58f,.53f,.38f),2.f);
 for(const auto& River : CryptWorld::Rivers)
 {
  FVector2D Previous=ToMap(FVector(River.X(River.Start),River.Start,0));
  for(int32 Index=1;Index<=24;++Index)
  {
   const float Y=FMath::Lerp(River.Start,River.End,Index/24.f);
   const FVector2D Point=ToMap(FVector(River.X(Y),Y,0));
   MapLine(Previous,Point,RiverColor,1.7f);
   Previous=Point;
  }
 }
 const auto Landmark=[&](FVector2D World,const TCHAR* Label,float OffsetY,EIcon Shape)
 {
  const FVector2D Point=ToMap(FVector(World.X,World.Y,0));
  Icon(Shape,Point,5.7f*S,Gold);
  float Width=0,Height=0;
  GetTextSize(Label,Width,Height,nullptr,.87f*S);
  DrawText(Label,FLinearColor::White,Point.X-Width*.72f,Point.Y+OffsetY*S,nullptr,.87f*S,false);
 };
 for(int32 I=0;I<CryptDungeons::Count;++I)
 {
  const FVector Location=CryptDungeons::Entrance(I);
  if(!bOverview && (Location.X<CryptWorld::West || Location.X>CryptWorld::East ||
                    FMath::Abs(Location.Y)>CryptWorld::North)) continue;
  const FVector2D Point=ToMap(Location);
  Icon(EIcon::Storm,Point,4.4f*S,FLinearColor(.78f,.54f,1.f));
 }
 Landmark(CryptWorld::Town,TEXT("Town"),7.f,EIcon::Town);
 Landmark(CryptWorld::Castle,TEXT("Castle"),-23.f,EIcon::Castle);
 DrawText(TEXT("N"),FLinearColor::White,Origin.X+9*S,Origin.Y+4*S,nullptr,.9f*S,false);

 TArray<FCryptQuestMarker> Markers;
 Hero->GetQuestMapMarkers(Markers);
 for(const FCryptQuestMarker& Marker : Markers)
 {
  const FVector2D Point=ToMap(Marker.Location);
  const FVector2D A=Point+FVector2D(0,-5*S),B=Point+FVector2D(5*S,0);
  const FVector2D C=Point+FVector2D(0,5*S),D=Point+FVector2D(-5*S,0);
  MapLine(A,B,Gold,1.5f);
  MapLine(B,C,Gold,1.5f);
  MapLine(C,D,Gold,1.5f);
  MapLine(D,A,Gold,1.5f);
 }
 const FVector Position=Hero->GetActorLocation();
 const FVector Forward=Hero->GetActorForwardVector();
 const FVector2D Player=ToMap(Position);
 const FVector2D Direction=FVector2D(Forward.X,-Forward.Y).GetSafeNormal();
 const FVector2D Side(-Direction.Y,Direction.X);
 const FVector2D Tip=Player+Direction*8*S;
 const FVector2D Left=Player-Direction*4*S-Side*4*S;
 const FVector2D Right=Player-Direction*4*S+Side*4*S;
 MapLine(Tip,Left,FLinearColor::White,2.f);
 MapLine(Left,Right,FLinearColor::White,2.f);
 MapLine(Right,Tip,FLinearColor::White,2.f);

 }

 Frame(20*S,12*S,210*S,44*S,Ink,Bronze);
 Icon(WeaponIcon(Hero->EquippedWeapon),FVector2D(38*S,33*S),10*S,Gold);
 DrawText(WeaponName(Hero->EquippedWeapon),FLinearColor::White,55*S,17*S,nullptr,1.12f*S,false);
 DrawText(FString::Printf(TEXT("%d treasure"),Hero->TreasureCount),Gold,55*S,35*S,nullptr,.83f*S,false);
 const float XpFraction=Hero->GetLevel()==100?1.f:
    float(Hero->GetExperience())/Hero->ExperienceNeeded();
 DrawRect(Ink,20*S,56*S,210*S,18*S);
 DrawRect(FLinearColor(.24f,.44f,.66f),21*S,57*S,208*S*XpFraction,16*S);
 DrawText(Hero->GetLevel()==100?FString(TEXT("Level 100")):
    FString::Printf(TEXT("Level %d  |  %d / %d XP"),Hero->GetLevel(),Hero->GetExperience(),Hero->ExperienceNeeded()),
    FLinearColor::White,28*S,58*S,nullptr,.77f*S,false);
 if(Hero->EquippedWeapon==ECryptWeapon::Bow)
  DrawText(Hero->GetArrowLabel(),Gold,28*S,76*S,nullptr,.79f*S,false);

 const float Target=Hero->bInventoryOpen?1.f:0.f;
 SatchelOpen=FMath::FInterpConstantTo(SatchelOpen,Target,GetWorld()->GetDeltaSeconds(),4.3f);
 if(SatchelOpen>0.f)
 {
  const float Open=FMath::SmoothStep(0.f,1.f,SatchelOpen);
  const FVector2D Center=FMath::Lerp(Layout.Bag,Layout.InSatchel(.5f,.50f),Open);
  const FVector2D AnimatedSize=FMath::Lerp(FVector2D(Layout.Radius*1.6f),Layout.SatchelSize,Open);
  DrawRect(FLinearColor(.01f,.008f,.006f,.61f*Open),Layout.SatchelOrigin.X-10*S,
   Layout.SatchelOrigin.Y,Layout.SatchelSize.X+20*S,Layout.SatchelSize.Y);
  if(SatchelTexture)
  {
   Sprite(0,Center,AnimatedSize,1.f-Open);
   Sprite(1,Center,AnimatedSize,Open);
  }
  else
  {
   Frame(Center.X-AnimatedSize.X*.5f,Center.Y-AnimatedSize.Y*.5f,AnimatedSize.X,AnimatedSize.Y,Ink,Bronze);
   Icon(EIcon::Bag,Center,AnimatedSize.Y*.18f,Gold);
  }

  if(SatchelOpen>.88f && Hero->bInventoryOpen)
  {
   const auto Pocket=[&](FVector2D Point,bool bSelected)
   {
    const float R=Layout.ItemRadius;
    const FLinearColor Color=bSelected?Gold:FLinearColor(.59f,.40f,.23f,.64f);
    const FVector2D A=Point-FVector2D(R,R),B=Point+FVector2D(R,-R);
    const FVector2D C=Point+FVector2D(R,R),D=Point+FVector2D(-R,R);
    DrawRect(FLinearColor(.027f,.014f,.009f,.77f),A.X,A.Y,2*R,2*R);
    Stroke(A,B,Color); Stroke(B,C,Color); Stroke(C,D,Color); Stroke(D,A,Color);
   };
   const auto CountBadge=[&](FVector2D Point,int32 Count)
   {
    const float R=Layout.ItemRadius;
    const FString Digits=FString::FromInt(Count);
    float Width=0,Height=0;
    GetTextSize(Digits,Width,Height,nullptr,.91f*S);
    DrawRect(Ink,Point.X+R-Width-6*S,Point.Y+R-Height-4*S,Width+6*S,Height+4*S);
    DrawText(Digits,Gold,Point.X+R-Width-3*S,Point.Y+R-Height-2*S,nullptr,.91f*S,false);
   };

   const auto GearControl=[&](FVector2D Position,const FString& Label,bool bSelected=false)
   {
    const float DX=Layout.SatchelSize.X*.085f;
    const float DY=Layout.SatchelSize.Y*.045f;
    Frame(Position.X-DX,Position.Y-DY,DX*2,DY*2,Ink,bSelected?Gold:Bronze);
    float LabelWidth=0,LabelHeight=0;
    const float TextScale=FMath::Min(.94f*S,DX/30.f);
    GetTextSize(Label,LabelWidth,LabelHeight,nullptr,TextScale);
    DrawText(Label,FLinearColor::White,Position.X-LabelWidth*.5f,Position.Y-LabelHeight*.5f,nullptr,TextScale,false);
   };
   if(Hero->bGearPage)
   {
    const FVector2D Top=Layout.InSatchel(.09f,.32f);
    const FVector2D Bottom=Layout.InSatchel(.91f,.84f);
    Frame(Top.X,Top.Y,Bottom.X-Top.X,Bottom.Y-Top.Y,Ink,Bronze);
    DrawText(Hero->GetEquippedGear(),Gold,Top.X+10*S,Top.Y+8*S,nullptr,.80f*S,false);
    if(Hero->MerchantId!=INDEX_NONE)
    {
     const FVector2D Stock=Layout.InSatchel(.13f,.40f);
     DrawText(Hero->GetMerchantStock(),FLinearColor::White,Stock.X,Stock.Y,nullptr,.87f*S,false);
     GearControl(Layout.InSatchel(.59f,.51f),TEXT("Next stock"));
     GearControl(Layout.InSatchel(.79f,.51f),TEXT("Buy"));
    }
    TArray<FString> ItemLines;
    Hero->GetGearSelection().ParseIntoArrayLines(ItemLines);
    const FVector2D Gear=Layout.InSatchel(.13f,.565f);
    for(int32 I=0;I<ItemLines.Num();++I)
     DrawText(ItemLines[I],FLinearColor::White,Gear.X,Gear.Y+15*S*I,nullptr,.86f*S,false);
    GearControl(Layout.GearButton(0),TEXT("Previous"));
    GearControl(Layout.GearButton(1),TEXT("Next"));
    GearControl(Layout.GearButton(2),Hero->GetGearAction(),true);
    if(Hero->MerchantId!=INDEX_NONE) GearControl(Layout.GearButton(3),TEXT("Sell"));
    GearControl(Layout.InSatchel(.19f,.79f),TEXT("Equip bow"),Hero->EquippedWeapon==ECryptWeapon::Bow);
    GearControl(Layout.InSatchel(.39f,.79f),Hero->GetArrowLabel());
    if(Hero->MerchantId!=INDEX_NONE) GearControl(Layout.InSatchel(.79f,.79f),TEXT("Buy arrows"));
   }
   else if(Hero->bSatchelJournal)
   {
    const FVector2D Top=Layout.InSatchel(.12f,.29f);
    const FVector2D Bottom=Layout.InSatchel(.88f,.78f);
    Frame(Top.X,Top.Y,Bottom.X-Top.X,Bottom.Y-Top.Y,FLinearColor(.24f,.15f,.085f),Gold);
    DrawText(TEXT("Field journal"),Gold,Top.X+13*S,Top.Y+9*S,nullptr,1.35f*S,false);
    TArray<FString> Entries;
    Hero->GetQuestJournal().ParseIntoArrayLines(Entries);
    const float JournalScale=FMath::Min(.95f*S,float(Bottom.X-Top.X)/400.f);
    for(int32 Index=0;Index<Entries.Num();++Index)
     DrawText(Entries[Index],FLinearColor::White,Top.X+13*S,Top.Y+(40+19*Index)*S,nullptr,JournalScale,false);
    Sprite(2,Layout.SupplyPocket(0),FVector2D(1.9f*Layout.ItemRadius));
   }
   else
   {
    const float R=Layout.ItemRadius;
    const int32 WeaponSprites[]={INDEX_NONE,5,6,7};
    for(int32 Index=0;Index<4;++Index)
    {
     const FVector2D Point=Layout.WeaponPocket(Index);
     const auto Weapon=static_cast<ECryptWeapon>(Index);
     Pocket(Point,Hero->EquippedWeapon==Weapon);
     if(!Hero->OwnsWeapon(Weapon)) continue;
     if(WeaponSprites[Index]!=INDEX_NONE && SatchelTexture)
      Sprite(WeaponSprites[Index],Point,FVector2D(1.96f*R));
     else Icon(WeaponIcon(Weapon),Point,.59f*R,Gold);
    }
    const int32 SupplySprites[]={2,9,8,10,11};
    const int32 Counts[]={1,1,Hero->GetPackedObjectiveCount(1),Hero->GetPackedObjectiveCount(2),Hero->GetPackedObjectiveCount(4)};
    for(int32 Index=0;Index<5;++Index)
    {
     if(Counts[Index]<=0) continue;
     const FVector2D Point=Layout.SupplyPocket(Index);
     Pocket(Point,false);
     Sprite(SupplySprites[Index],Point,FVector2D(1.96f*R));
     if(Index==1) CountBadge(Point,Hero->TreasureCount);
     if(Index==2) CountBadge(Point,Counts[Index]);
    }
   }

   if(!Hero->bSatchelJournal && !Hero->bGearPage)
   {
    const FVector2D Heading=Layout.InSatchel(.12f,.722f);
    DrawText(TEXT("Right thumb tap"),Gold,Heading.X,Heading.Y,nullptr,1.08f*S,false);
    const float HalfWidth=Layout.SatchelSize.X*.075f;
    const float HalfHeight=Layout.SatchelSize.Y*.040f;
    for(int32 Index=0;Index<int32(ECryptThumbAction::Count);++Index)
    {
     const FVector2D Option=Layout.ThumbActionOption(Index);
     const bool bAssigned=Index==int32(Hero->GetThumbAction());
     Frame(Option.X-HalfWidth,Option.Y-HalfHeight,2*HalfWidth,2*HalfHeight,
       bAssigned?FLinearColor(.37f,.27f,.14f,.99f):Ink,bAssigned?Gold:Bronze);
     float LabelWidth=0,LabelHeight=0;
     const float TextScale=FMath::Min(.99f*S,HalfWidth/31.f);
     GetTextSize(CryptThumbActionLabels[Index],LabelWidth,LabelHeight,nullptr,TextScale);
     DrawText(CryptThumbActionLabels[Index],FLinearColor::White,Option.X-LabelWidth*.5f,
       Option.Y-LabelHeight*.5f,nullptr,TextScale,false);
    }
   }

   if(!Hero->bSatchelJournal && !Hero->bGearPage)
   {
    const FVector2D Relic=Layout.InSatchel(.17f,.30f);
    DrawText(Hero->GetRelicSummary(),Gold,Relic.X,Relic.Y,nullptr,.87f*S,false);
   }
   const auto Utility=[&](FVector2D Point,EIcon Shape)
   {
    const float R=Layout.UtilityRadius;
    Frame(Point.X-R,Point.Y-R,2*R,2*R,Ink,Bronze);
    Icon(Shape,Point,.52f*R,Gold);
   };
   GearControl(Layout.GearTab(),Hero->bGearPage?TEXT("Bag"):TEXT("Loot & gear"),Hero->bGearPage);
   Utility(Layout.Save,EIcon::Save);
   Utility(Layout.Restore,EIcon::Restore);
   const FString& Message=Hero->InspectedItem.IsEmpty()?Hero->SaveStatus:Hero->InspectedItem;
   if(!Message.IsEmpty())
   {
    float Width=0,Height=0;
    GetTextSize(Message,Width,Height,nullptr,.98f*S);
    const FVector2D Point=Layout.InSatchel(.5f,.95f);
    DrawText(Message,Gold,Point.X-.5f*Width,Point.Y-.5f*Height,nullptr,.98f*S,false);
   }
  }
  if(Hero->bInventoryOpen) return;
 }

 const auto& Log=Hero->GetEventLog();
 const int32 Offset=Hero->GetLogOffset();
 const FVector2D LogAt=Layout.LogOrigin;
 Frame(LogAt.X,LogAt.Y,Layout.LogSize.X,Layout.LogSize.Y,Ink,Bronze);
 DrawText(TEXT("Adventure log"),Gold,LogAt.X+9*S,LogAt.Y+6*S,nullptr,.9f*S,false);
 const auto Scroller=[&](FVector2D Point,bool bUp,bool bActive)
 {
  const FLinearColor Color=bActive?Gold:Bronze;
  Frame(Point.X-11*S,Point.Y-9*S,22*S,18*S,Ink,Color);
  const float Sign=bUp?-1.f:1.f;
  Stroke(Point+FVector2D(-4,Sign*-2)*S,Point+FVector2D(0,Sign*3)*S,Color);
  Stroke(Point+FVector2D(4,Sign*-2)*S,Point+FVector2D(0,Sign*3)*S,Color);
 };
 Scroller(Layout.LogOlder,true,Offset<FMath::Max(0,Log.Num()-3));
 Scroller(Layout.LogNewer,false,Offset>0);
 const int32 LogEnd=Log.Num()-Offset;
 const int32 LogStart=FMath::Max(0,LogEnd-3);
 for(int32 I=LogStart;I<LogEnd;++I)
 {
  FString Line=Log[I];
  float TextWidth=0,TextHeight=0;
  GetTextSize(Line,TextWidth,TextHeight,nullptr,.85f*S);
  if(TextWidth>Layout.LogSize.X-18*S)
  {
   const int32 Limit=FMath::Max(1,FMath::FloorToInt(Line.Len()*(Layout.LogSize.X-30*S)/TextWidth));
   Line=Line.Left(Limit)+TEXT("...");
  }
  DrawText(Line,FLinearColor::White,LogAt.X+9*S,LogAt.Y+(30+20*(I-LogStart))*S,nullptr,.85f*S,false);
 }

 if(const auto* Enemy=Hero->GetCombatEnemy())
 {
  const float Width=FMath::Min(270.f*S,W*.25f);
  const float X=(W-Width)*.5f,Y=H*.155f;
  const float Progress=FMath::Clamp(Enemy->GetRemainingHealth()/Enemy->Stats().Health,0.f,1.f);
  Frame(X,Y,Width,30*S,Ink,Enemy->IsBoss()?Gold:Bronze);
  DrawRect(FLinearColor(.67f,.16f,.12f),X+3*S,Y+20*S,(Width-6*S)*Progress,6*S);
  DrawText(FString::Printf(TEXT("%s%s  %d / %d"),Enemy->IsBoss()?TEXT("Boss: "):TEXT(""),
    Enemy->Stats().Name,FMath::CeilToInt(Enemy->GetRemainingHealth()),FMath::CeilToInt(Enemy->Stats().Health)),
   FLinearColor::White,X+7*S,Y+3*S,nullptr,.91f*S,false);
 }

 const FString Nearby=Hero->GetInteractionLabel();
 if(!Nearby.IsEmpty())
 {
  Button(Layout.Interact,EIcon::Talk,TEXT("INTERACT"));
  DrawText(Nearby,Gold,38*S,Layout.Interact.Y+Layout.Radius+6*S,nullptr,1.05f*S,false);
 }

 TArray<FString> Entries;
 Hero->GetQuestJournal().ParseIntoArrayLines(Entries);
 for(int32 Index=0;Index<Entries.Num();++Index)
  DrawText(Entries[Index],FLinearColor::White,30*S,(Hero->EquippedWeapon==ECryptWeapon::Bow?95+20*Index:78+20*Index)*S,nullptr,1.0f*S,false);

 if(!Hero->Dialogue.IsEmpty())
 {
  TArray<FString> Words;
  Hero->Dialogue.ParseIntoArrayWS(Words);
  TArray<FString> Lines;
  FString Line;
  for(const FString& Word : Words)
  {
   const FString Candidate=Line.IsEmpty()?Word:Line+TEXT(" ")+Word;
   float Width=0,Height=0;
   GetTextSize(Candidate,Width,Height,nullptr,1.25f*S);
   if(!Line.IsEmpty() && Width>W*.50f)
   {
    Lines.Add(Line);
    Line=Word;
   }
   else Line=Candidate;
  }
  Lines.Add(Line);
  const float Top=H*.72f-(Lines.Num()-1)*20*S;
  DrawRect(Panel,W*.23f,Top-11*S,W*.54f,(Lines.Num()*20+22)*S);
  for(int32 Index=0;Index<Lines.Num();++Index)
   DrawText(Lines[Index],Gold,W*.25f,Top+Index*20*S,nullptr,1.25f*S,false);
 }
}
