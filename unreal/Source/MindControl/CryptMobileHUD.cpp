#include "CryptMobileHUD.h"
#include "CryptHero.h"
#include "Engine/Canvas.h"

void ACryptMobileHUD::DrawHUD()
{
 Super::DrawHUD();
 if(!Canvas) return;
 const float W=Canvas->SizeX, H=Canvas->SizeY;
 const float S=FMath::Max(.7f,FMath::Min(W/1600.f,H/900.f));
 const FLinearColor Panel(.035f,.055f,.08f,.9f);
 const FLinearColor Gold(.96f,.79f,.44f);
 const auto Button=[&](float X,float Y,const TCHAR* Label)
 {
  const float R=74.f*S;
  DrawRect(Panel,X-R,Y-R,R*2,R*2);
  DrawText(Label,FLinearColor::White,X-R+12*S,Y-13*S,nullptr,1.25f*S,false);
 };
 Button(W-110*S,105*S,TEXT("JUMP"));
 Button(W-280*S,105*S,TEXT("ATTACK"));
 Button(W-110*S,275*S,TEXT("SPIN"));
 Button(W-280*S,275*S,TEXT("BAG"));

 const ACryptHero* Hero=Cast<ACryptHero>(GetOwningPawn());
 if(!Hero) return;
 DrawText(FString::Printf(TEXT("Treasure: %d"),Hero->TreasureCount),Gold,30*S,25*S,nullptr,1.3f*S,false);

 if(Hero->bInventoryOpen)
 {
  const TCHAR* Weapons[]={TEXT("Unarmed"),TEXT("Longsword"),TEXT("Battle axe"),TEXT("Spear")};
  DrawRect(Panel,W*.22f,H*.14f,W*.44f,H*.64f);
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
  return;
 }

 const FString Nearby=Hero->GetInteractionLabel();
 if(!Nearby.IsEmpty())
 {
  Button(110*S,210*S,TEXT("INTERACT"));
  DrawText(Nearby,Gold,38*S,293*S,nullptr,1.05f*S,false);
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
