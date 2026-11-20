#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CryptTownActor.generated.h"

class USceneComponent;
class USkeletalMeshComponent;
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

private:
 UPROPERTY() USceneComponent* VisualRoot;
 UPROPERTY() USkeletalMeshComponent* Person;
 UPROPERTY() UStaticMeshComponent* Marker;
};
