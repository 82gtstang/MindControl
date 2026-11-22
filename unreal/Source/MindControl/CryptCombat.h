#pragma once
#include "CoreMinimal.h"

enum class ECryptDamage : uint8 { Physical, Fire, Storm, Poison };

namespace CryptCombat
{
 struct FCreature
 {
  const TCHAR* Name;
  float Health, Stamina, Mana, Dexterity, Armor, Weapon;
  float Fire, Storm; // Damage multipliers. Below 1 resists, above 1 is weak.
  float Speed;
  ECryptDamage Element;
  FLinearColor Color;
 };

 inline const FCreature Creatures[] = {
  {TEXT("Road raider"), 30,45,0,12,2,8,1,1,170,ECryptDamage::Physical,{.32f,.18f,.10f}},
  {TEXT("Grave sentinel"), 45,55,0,8,5,10,1.25f,.8f,125,ECryptDamage::Physical,{.45f,.46f,.37f}},
  {TEXT("Cave spider"), 24,58,0,27,1,8,1.6f,.9f,285,ECryptDamage::Physical,{.18f,.09f,.23f}},
  {TEXT("Ash imp"), 34,42,50,18,2,10,.42f,1.6f,215,ECryptDamage::Fire,{.72f,.13f,.045f}},
  {TEXT("Silt crawler"), 49,55,0,10,7,12,.9f,1.7f,135,ECryptDamage::Physical,{.19f,.34f,.19f}},
  {TEXT("Thunder bat"), 35,36,64,24,0,10,1.45f,.40f,245,ECryptDamage::Storm,{.16f,.42f,.83f}},
  {TEXT("Iron construct"), 76,68,0,6,14,17,.65f,1.75f,104,ECryptDamage::Physical,{.28f,.30f,.37f}},
  {TEXT("Frost ghoul"), 56,50,40,14,6,14,1.85f,.74f,157,ECryptDamage::Storm,{.35f,.66f,.75f}},
  {TEXT("Ember drake"), 82,64,60,16,9,16,.35f,1.48f,183,ECryptDamage::Fire,{.65f,.22f,.065f}},
  {TEXT("Void knight"), 102,78,48,20,12,19,1.40f,.58f,172,ECryptDamage::Storm,{.16f,.12f,.23f}},
  {TEXT("Root guardian"), 72,62,0,9,10,15,1.75f,.66f,126,ECryptDamage::Physical,{.28f,.43f,.12f}},
  {TEXT("Vault keeper"), 130,88,72,18,16,22,.74f,1.35f,150,ECryptDamage::Fire,{.60f,.41f,.14f}},
  {TEXT("Briar hound"), 43,70,0,24,2,11,1.35f,.88f,252,ECryptDamage::Physical,{.27f,.26f,.24f}},
  {TEXT("Ironback boar"), 61,65,0,13,8,15,.92f,1.15f,204,ECryptDamage::Physical,{.41f,.23f,.11f}},
  {TEXT("Crypt wraith"), 48,48,55,22,1,13,1.46f,.67f,174,ECryptDamage::Storm,{.26f,.45f,.62f}},
  {TEXT("Chalkscale lizard"), 39,62,0,26,5,10,.76f,1.32f,236,ECryptDamage::Physical,{.25f,.48f,.30f}},
  {TEXT("Ember scorpion"), 53,57,40,19,7,13,.50f,1.42f,185,ECryptDamage::Fire,{.58f,.23f,.12f}},
  {TEXT("Brineclaw crab"), 57,60,0,12,11,14,1.27f,.91f,136,ECryptDamage::Physical,{.31f,.44f,.53f}},
  {TEXT("Spore husk"), 54,55,0,11,4,13,1.64f,.83f,144,ECryptDamage::Physical,{.46f,.36f,.21f}},
  {TEXT("Crystal bulwark"), 85,69,44,10,13,16,1.09f,.58f,115,ECryptDamage::Storm,{.20f,.54f,.76f}}
 };
 inline float Damage(float Rating,float Armor,ECryptDamage Element)
 {
  const float EffectiveArmor=Armor*(Element==ECryptDamage::Physical?1.f:.35f);
  return FMath::Max(1.f,Rating*30.f/(30.f+EffectiveArmor));
 }
 inline float HitChance(float AttackerDexterity,float DefenderDexterity)
 {
  return FMath::Clamp(.89f+(AttackerDexterity-DefenderDexterity)*.009f,.65f,.98f);
 }
}
