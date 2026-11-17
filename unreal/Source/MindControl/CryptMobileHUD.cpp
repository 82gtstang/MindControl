#include "CryptMobileHUD.h"
#include "CryptHero.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
void ACryptMobileHUD::DrawHUD()
{
 Super::DrawHUD();
 if(!Canvas) return;
 const float W=Canvas->SizeX, H=Canvas->SizeY;
 const float S=FMath::Max(0.7f,FMath::Min(W/1600.f,H/900.f));
 auto Button=[&](float X,float Y,const FString& Label)
 {
  const float R=74.f*S;
  DrawRect(FLinearColor(0.07f,0.12f,0.24f,0.75f),X-R,Y-R,R*2,R*2);
  DrawText(Label,FLinearColor::White,X-R+12*S,Y-13*S,nullptr,1.25f*S,false);
 };
 Button(W-110*S,105*S,TEXT("JUMP"));
 Button(W-280*S,105*S,TEXT("ATTACK"));
 Button(W-110*S,275*S,TEXT("SPIN"));
 Button(W-280*S,275*S,TEXT("BAG"));
 if(const ACryptHero* Hero=Cast<ACryptHero>(GetOwningPawn()))
 {
  DrawText(FString::Printf(TEXT("Treasure: %d"),Hero->TreasureCount),FLinearColor::Yellow,30*S,25*S,nullptr,1.3f*S,false);
  if(Hero->bInventoryOpen)
  {
   DrawRect(FLinearColor(0.02f,0.03f,0.07f,0.88f),W*.22f,H*.12f,W*.55f,H*.72f);
   DrawText(TEXT("INVENTORY / EQUIPMENT"),FLinearColor::Yellow,W*.26f,H*.19f,nullptr,2*S,false);
   DrawText(FString::Printf(TEXT("Treasure: %d"),Hero->TreasureCount),FLinearColor::White,W*.26f,H*.30f,nullptr,1.5f*S,false);
   DrawText(FString::Printf(TEXT("Weapon: %s"),Hero->bSwordEquipped?TEXT("Sword"):TEXT("Unarmed")),FLinearColor::White,W*.26f,H*.39f,nullptr,1.5f*S,false);
   DrawText(TEXT("Tap BAG to close"),FLinearColor::White,W*.26f,H*.55f,nullptr,1.2f*S,false);
  }
 }
}
