#include "CryptMobileHUD.h"
#include "CryptHero.h"
#include "CryptHudLayout.h"
#include "CryptWorldLayout.h"
#include "Engine/Canvas.h"

void ACryptMobileHUD::DrawHUD()
{
 Super::DrawHUD();
 if(!Canvas) return;
 const float W=Canvas->SizeX, H=Canvas->SizeY;
 const FCryptHudLayout Layout(W,H);
 const float S=Layout.Scale;
 const FLinearColor Panel(.035f,.055f,.08f,.9f);
 const FLinearColor Gold(.96f,.79f,.44f);
 const auto Button=[&](const FVector2D& Center,const TCHAR* Label)
 {
  const float R=Layout.Radius;
  DrawRect(Panel,Center.X-R,Center.Y-R,R*2,R*2);
  float Width=0,Height=0;
  GetTextSize(Label,Width,Height,nullptr,1.1f*S);
  DrawText(Label,FLinearColor::White,Center.X-Width*.5,Center.Y-Height*.5,nullptr,1.1f*S,false);
 };
 Button(Layout.Jump,TEXT("JUMP"));
 Button(Layout.Attack,TEXT("ATTACK"));
 Button(Layout.Spin,TEXT("SPIN"));
 Button(Layout.Bag,TEXT("BAG"));

 const ACryptHero* Hero=Cast<ACryptHero>(GetOwningPawn());
 if(!Hero) return;

 const FVector2D Origin=Layout.MapOrigin,Size=Layout.MapSize;
 const FLinearColor RiverColor(.31f,.72f,.88f);
 DrawRect(FLinearColor(.057f,.10f,.067f,.94f),Origin.X,Origin.Y,Size.X,Size.Y);
 const auto ToMap=[&](const FVector& Location)
 {
  const FVector2D Unit=CryptWorld::MapPoint(Location);
  return Origin+FVector2D(8*S+Unit.X*(Size.X-16*S),8*S+Unit.Y*(Size.Y-16*S));
 };
 const auto Line=[&](const FVector2D& A,const FVector2D& B,const FLinearColor& Color,float Thickness)
 {
  DrawLine(A.X,A.Y,B.X,B.Y,Color,Thickness*S);
 };
 Line(ToMap(FVector(-5500,0,0)),ToMap(FVector(5500,0,0)),FLinearColor(.58f,.53f,.38f),2.f);
 for(const auto& River : CryptWorld::Rivers)
 {
  FVector2D Previous=ToMap(FVector(River.X(River.Start),River.Start,0));
  for(int32 Index=1;Index<=24;++Index)
  {
   const float Y=FMath::Lerp(River.Start,River.End,Index/24.f);
   const FVector2D Point=ToMap(FVector(River.X(Y),Y,0));
   Line(Previous,Point,RiverColor,1.7f);
   Previous=Point;
  }
 }
 const auto Landmark=[&](FVector2D World,const TCHAR* Label,float OffsetY)
 {
  const FVector2D Point=ToMap(FVector(World.X,World.Y,0));
  DrawRect(FLinearColor(.83f,.85f,.76f),Point.X-3*S,Point.Y-3*S,6*S,6*S);
  float Width=0,Height=0;
  GetTextSize(Label,Width,Height,nullptr,.87f*S);
  DrawText(Label,FLinearColor::White,Point.X-Width*.72f,Point.Y+OffsetY*S,nullptr,.87f*S,false);
 };
 Landmark(CryptWorld::Town,TEXT("Town"),7.f);
 Landmark(CryptWorld::Castle,TEXT("Castle"),-23.f);
 DrawText(TEXT("N"),FLinearColor::White,Origin.X+9*S,Origin.Y+4*S,nullptr,.9f*S,false);

 TArray<FCryptQuestMarker> Markers;
 Hero->GetQuestMapMarkers(Markers);
 for(const FCryptQuestMarker& Marker : Markers)
 {
  const FVector2D Point=ToMap(Marker.Location);
  const FVector2D A=Point+FVector2D(0,-5*S),B=Point+FVector2D(5*S,0);
  const FVector2D C=Point+FVector2D(0,5*S),D=Point+FVector2D(-5*S,0);
  Line(A,B,Gold,1.5f);
  Line(B,C,Gold,1.5f);
  Line(C,D,Gold,1.5f);
  Line(D,A,Gold,1.5f);
 }
 const FVector Position=Hero->GetActorLocation();
 const FVector Forward=Hero->GetActorForwardVector();
 const FVector2D Player=ToMap(Position);
 const FVector2D Direction=FVector2D(Forward.X,-Forward.Y).GetSafeNormal();
 const FVector2D Side(-Direction.Y,Direction.X);
 const FVector2D Tip=Player+Direction*8*S;
 const FVector2D Left=Player-Direction*4*S-Side*4*S;
 const FVector2D Right=Player-Direction*4*S+Side*4*S;
 Line(Tip,Left,FLinearColor::White,2.f);
 Line(Left,Right,FLinearColor::White,2.f);
 Line(Right,Tip,FLinearColor::White,2.f);

 DrawText(FString::Printf(TEXT("Treasure: %d"),Hero->TreasureCount),Gold,30*S,25*S,nullptr,1.3f*S,false);

 if(Hero->bInventoryOpen)
 {
  const TCHAR* Weapons[]={TEXT("Unarmed"),TEXT("Longsword"),TEXT("Battle axe"),TEXT("Spear")};
  DrawRect(Panel,W*.22f,H*.14f,W*.44f,H*.72f);
  DrawText(TEXT("EQUIPMENT"),Gold,W*.26f,H*.18f,nullptr,1.65f*S,false);
  for(int32 Index=0;Index<4;++Index)
  {
   const auto Weapon=static_cast<ECryptWeapon>(Index);
   const bool bOwned=Hero->OwnsWeapon(Weapon);
   const bool bEquipped=Weapon==Hero->EquippedWeapon;
   const float Y=H*.30f+Index*H*.10f;
   DrawRect(FLinearColor(.13f,.19f,.25f,.9f),W*.25f,Y-H*.045f,W*.37f,H*.09f);
   DrawText(FString::Printf(TEXT("%s%s"),Weapons[Index],bEquipped?TEXT("  (equipped)"):(!bOwned?TEXT("  (quest reward)"):TEXT(""))),
     bEquipped?Gold:(bOwned?FLinearColor::White:FLinearColor(.52f,.57f,.62f)),W*.26f,Y-9*S,nullptr,1.25f*S,false);
  }
  DrawRect(FLinearColor(.18f,.24f,.25f),W*.25f,H*.67f,W*.175f,H*.08f);
  DrawRect(FLinearColor(.18f,.24f,.25f),W*.445f,H*.67f,W*.175f,H*.08f);
  DrawText(TEXT("Save"),Gold,W*.27f,H*.68f,nullptr,1.15f*S,false);
  DrawText(TEXT("Restore"),Gold,W*.465f,H*.68f,nullptr,1.15f*S,false);
  if(!Hero->SaveStatus.IsEmpty())
   DrawText(Hero->SaveStatus,FLinearColor::White,W*.25f,H*.78f,nullptr,.95f*S,false);
  return;
 }

 const FString Nearby=Hero->GetInteractionLabel();
 if(!Nearby.IsEmpty())
 {
  Button(Layout.Interact,TEXT("INTERACT"));
  DrawText(Nearby,Gold,38*S,Layout.Interact.Y+Layout.Radius+6*S,nullptr,1.05f*S,false);
 }

 TArray<FString> Entries;
 Hero->GetQuestJournal().ParseIntoArrayLines(Entries);
 for(int32 Index=0;Index<Entries.Num();++Index)
  DrawText(Entries[Index],FLinearColor::White,30*S,(52+20*Index)*S,nullptr,1.0f*S,false);

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
