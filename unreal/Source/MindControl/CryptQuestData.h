#pragma once

#include "CoreMinimal.h"

enum class ECryptWeapon : uint8 { Unarmed, Sword, Axe, Spear, Bow };

namespace CryptQuests
{
 struct FQuest
 {
  const TCHAR* Title;
  const TCHAR* Giver;
  const TCHAR* Offer;
  const TCHAR* Objective;
  const TCHAR* Thanks;
  int32 Target;
  int32 Treasure;
  ECryptWeapon Reward;
 };

 inline constexpr int32 Count = 5;
 inline const FQuest Data[Count] = {
  {TEXT("The Road Patrol"), TEXT("Mara, Captain"), TEXT("Three raiders are watching the western road. Clear it before the carts arrive."), TEXT("Defeat the three western road raiders"), TEXT("The supply carts can cross again. Take this longsword."), 3, 12, ECryptWeapon::Sword},
  {TEXT("Herbs for the Healer"), TEXT("Iona, Healer"), TEXT("I need three sprigs from the southern valley. Search the low ground and the slopes."), TEXT("Gather three herbs in the southern valley"), TEXT("These will last us through winter. Take this spear."), 3, 8, ECryptWeapon::Spear},
  {TEXT("The Lost Crest"), TEXT("Bren, Archivist"), TEXT("Our old crest is still in the northern ruins. Enter from the south and follow the paving."), TEXT("Recover the crest from the northern ruins"), TEXT("I thought we'd lost it for good. The smith sent this axe for you."), 1, 15, ECryptWeapon::Axe},
  {TEXT("Watch on the Ridge"), TEXT("Tessa, Scout"), TEXT("Find my survey marker on the western ridge, south of the road. Tell me if the trail is clear."), TEXT("Inspect the survey marker on the western ridge"), TEXT("Good. I'll send a patrol up the ridge at dawn."), 1, 10, ECryptWeapon::Unarmed},
  {TEXT("The Missing Pay Chest"), TEXT("Leif, Quartermaster"), TEXT("A wagon lost its pay chest below the eastern hillside. Search southeast of the square."), TEXT("Recover the pay chest below the eastern hill"), TEXT("Every guard in town will be glad to see this."), 1, 25, ECryptWeapon::Unarmed}
 };
}
