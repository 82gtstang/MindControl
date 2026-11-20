#include "CryptTownActor.h"
#include "Animation/AnimSequence.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ACryptTownActor::ACryptTownActor()
{
 VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
 SetRootComponent(VisualRoot);
 Person = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Person"));
 Person->SetupAttachment(VisualRoot);
 Person->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Person->SetVisibility(false);
 Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Objective"));
 Marker->SetupAttachment(VisualRoot);
 Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);

 static ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
 Marker->SetStaticMesh(Shape.Object);
}

void ACryptTownActor::Configure(int32 InQuestIndex, bool bInQuestGiver)
{
 QuestIndex = InQuestIndex;
 bQuestGiver = bInQuestGiver;
 if(bQuestGiver)
 {
  USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/OceanSerenity/OceanSerenity.OceanSerenity"));
  if(Mesh)
  {
   Person->SetSkeletalMeshAsset(Mesh);
   const FBoxSphereBounds Bounds = Mesh->GetBounds();
   const float Scale = (164.f + 5.f * QuestIndex) / FMath::Max(1.f, float(Bounds.BoxExtent.Z * 2));
   Person->SetRelativeScale3D(FVector(Scale));
   Person->SetRelativeLocation(FVector(0,0,-(Bounds.Origin.Z-Bounds.BoxExtent.Z)*Scale));
   Person->SetRelativeRotation(FRotator(0,-90,0));
   Person->SetVisibility(true);
   Marker->SetVisibility(false);
   UAnimSequence* Idle = LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/OceanSerenity/OceanSerenityOcean_Idle.OceanSerenityOcean_Idle"));
   if(Idle) Person->PlayAnimation(Idle,true);
   return;
  }
 }
 Marker->SetRelativeLocation(FVector(0,0,bQuestGiver?70:18));
 Marker->SetRelativeScale3D(bQuestGiver?FVector(.42f,.42f,1.45f):FVector(.42f,.29f,.34f));
 if(UMaterialInterface* Stone=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Environment/MindControl/M_CastleStone.M_CastleStone")))
  Marker->SetMaterial(0,Stone);
}

