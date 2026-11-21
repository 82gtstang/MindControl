#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CryptTownActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class MINDCONTROL_API ACryptTownActor : public AActor
{
 GENERATED_BODY()

public:
 ACryptTownActor();

 void Configure(int32 QuestIndex, bool bQuestGiver);
 int32 QuestIndex = INDEX_NONE;
 bool bQuestGiver = false;
 int32 ObjectiveId = INDEX_NONE;
 bool bCollected = false;
 void SetCollected(bool bValue)
 {
  bCollected=bValue;
  SetActorHiddenInGame(bCollected);
 }

private:
 UPROPERTY() USceneComponent* VisualRoot;
 UPROPERTY() UStaticMeshComponent* Person;
 UPROPERTY() UStaticMeshComponent* Marker;
};
