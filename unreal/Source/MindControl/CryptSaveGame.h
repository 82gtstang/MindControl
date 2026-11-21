#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "CryptSaveGame.generated.h"

UCLASS()
class MINDCONTROL_API UCryptSaveGame : public USaveGame
{
 GENERATED_BODY()

public:
 UPROPERTY(SaveGame) int32 Version=1;
 UPROPERTY(SaveGame) FVector HeroLocation=FVector::ZeroVector;
 UPROPERTY(SaveGame) FRotator HeroRotation=FRotator::ZeroRotator;
 UPROPERTY(SaveGame) FRotator ViewRotation=FRotator::ZeroRotator;
 UPROPERTY(SaveGame) int32 Treasure=0;
 UPROPERTY(SaveGame) float HeroHealth=100.f;
 UPROPERTY(SaveGame) float HeroStamina=100.f;
 UPROPERTY(SaveGame) float HeroMana=75.f;
 UPROPERTY(SaveGame) int32 ClearedDungeons=0;
 UPROPERTY(SaveGame) TSet<int32> ChartTiles;
 UPROPERTY(SaveGame) TSet<int32> OpenSecrets;
 UPROPERTY(SaveGame) TArray<FString> EventLog;
 UPROPERTY(SaveGame) TArray<int32> Relics;
 UPROPERTY(SaveGame) int32 HeroLevel=1;
 UPROPERTY(SaveGame) int32 HeroExperience=0;
 UPROPERTY(SaveGame) int32 ArrowType=0;
 UPROPERTY(SaveGame) TArray<int32> Arrows;
 UPROPERTY(SaveGame) int32 LootWeapon=INDEX_NONE;
 UPROPERTY(SaveGame) int32 LootArmor=INDEX_NONE;
 UPROPERTY(SaveGame) int32 Weapons=1;
 UPROPERTY(SaveGame) uint8 Equipped=0;
 UPROPERTY(SaveGame) TArray<uint8> QuestStates;
 UPROPERTY(SaveGame) TArray<int32> QuestCounts;
 UPROPERTY(SaveGame) TArray<int32> CollectedObjectives;
 UPROPERTY(SaveGame) TMap<int32,float> EnemyHealth;
 UPROPERTY(SaveGame) TMap<int32,FVector> EnemyLocations;
};
