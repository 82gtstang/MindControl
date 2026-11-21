#include "CryptHero.h"
#include "CryptMonster.h"
#include "CryptTownActor.h"
#include "CryptSaveGame.h"
#include "CryptHudLayout.h"
#include "Materials/MaterialInterface.h"
#include "EngineUtils.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameViewportClient.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
ACryptHero::ACryptHero()
{
 PrimaryActorTick.bCanEverTick = true;
 GetCharacterMovement()->MaxWalkSpeed = 520;
 GetCharacterMovement()->JumpZVelocity = 650;
 GetCharacterMovement()->AirControl = .45f;
 CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
 CameraBoom->SetupAttachment(RootComponent);
 CameraBoom->TargetArmLength = 380;
 CameraBoom->bDoCollisionTest = true;
 CameraBoom->bUsePawnControlRotation = true;
 FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
 FollowCamera->SetupAttachment(CameraBoom);
 // Prefer the future photorealistic Ocean Serenity heroine, then the legacy rig.
 static ConstructorHelpers::FObjectFinder<USkeletalMesh> HeroineMesh(TEXT("/Game/Characters/OceanSerenityHeroine/OceanSerenityHeroine.OceanSerenityHeroine"));
 static ConstructorHelpers::FObjectFinder<USkeletalMesh> OceanMesh(TEXT("/Game/Characters/OceanSerenity/OceanSerenity.OceanSerenity"));
 // Load the licensed Countess mesh if its Fab content has been installed locally.
 static ConstructorHelpers::FObjectFinder<USkeletalMesh> CountessMesh(TEXT("/Game/ParagonCountess/Characters/Heroes/Countess/Meshes/Countess.Countess"));
 const bool bHasHeroine = HeroineMesh.Succeeded();
 const bool bHasOcean = bHasHeroine || OceanMesh.Succeeded();
 const bool bHasCountess = !bHasOcean && CountessMesh.Succeeded();
 if(bHasOcean)
 {
  GetMesh()->SetSkeletalMesh(bHasHeroine ? HeroineMesh.Object : OceanMesh.Object);
  UE_LOG(LogTemp,Warning,TEXT("MindControl heroine mesh source: %s"),bHasHeroine ? TEXT("OceanSerenityHeroine") : TEXT("legacy OceanSerenity"));
  // Blender rig is grounded at its feet, unlike the usual UE mannequin
  // whose mesh origin is at capsule center. Keep Ocean at capsule base
  // and allow a little clearance above the floor.
  GetMesh()->SetRelativeLocation(FVector(0,0,-75));
  // The image-based rig is already Z-up after import. Align its facing with the pawn.
  GetMesh()->SetRelativeRotation(FRotator(0,-90,0));
  const float MeshHeight = GetMesh()->GetSkeletalMeshAsset()->GetBounds().BoxExtent.Z * 2.f;
  if (MeshHeight > KINDA_SMALL_NUMBER)
  {
   const float CapsuleHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() * 2.f;
   GetMesh()->SetRelativeScale3D(FVector(CapsuleHeight / MeshHeight));
  }
  static ConstructorHelpers::FObjectFinder<UAnimSequence> Idle(TEXT("/Game/Characters/OceanSerenity/OceanSerenityOcean_Idle.OceanSerenityOcean_Idle"));
  static ConstructorHelpers::FObjectFinder<UAnimSequence> Walk(TEXT("/Game/Characters/OceanSerenity/OceanSerenityOcean_Walk.OceanSerenityOcean_Walk"));
  static ConstructorHelpers::FObjectFinder<UAnimSequence> Run(TEXT("/Game/Characters/OceanSerenity/OceanSerenityOcean_Run.OceanSerenityOcean_Run"));
  static ConstructorHelpers::FObjectFinder<UAnimSequence> JumpAnim(TEXT("/Game/Characters/OceanSerenity/OceanSerenityOcean_Jump.OceanSerenityOcean_Jump"));
  static ConstructorHelpers::FObjectFinder<UAnimSequence> AttackAnim(TEXT("/Game/Characters/OceanSerenity/OceanSerenityOcean_Attack.OceanSerenityOcean_Attack"));
  static ConstructorHelpers::FObjectFinder<UAnimSequence> SpinAnim(TEXT("/Game/Characters/OceanSerenity/OceanSerenityOcean_Spin.OceanSerenityOcean_Spin"));
  OceanIdle=Idle.Object; OceanWalk=Walk.Object; OceanRun=Run.Object;
  OceanJump=JumpAnim.Object; OceanAttack=AttackAnim.Object; OceanSpin=SpinAnim.Object;
  // Start the relaxed idle immediately; never leave the mesh in FBX reference pose.
  if (OceanIdle)
  {
   GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
   GetMesh()->PlayAnimation(OceanIdle,true);
   ActiveOceanAnimation=OceanIdle;
  }
  else
  {
   UE_LOG(LogTemp,Error,TEXT("Ocean Serenity idle animation missing: /Game/Characters/OceanSerenity/Ocean_Idle"));
  }
  UE_LOG(LogTemp,Warning,TEXT("Ocean Serenity clips loaded: idle=%d walk=%d run=%d jump=%d attack=%d spin=%d"),
   OceanIdle!=nullptr,OceanWalk!=nullptr,OceanRun!=nullptr,OceanJump!=nullptr,OceanAttack!=nullptr,OceanSpin!=nullptr);
 }
 if(bHasCountess)
 {
  GetMesh()->SetSkeletalMesh(CountessMesh.Object);
  GetMesh()->SetRelativeLocation(FVector(0,0,-90));
  GetMesh()->SetRelativeRotation(FRotator(0,-90,0));
 }
 // Keep the fallback silhouette when the Fab pack is absent.
 UStaticMeshComponent* Torso=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeroTorso"));
 Torso->SetupAttachment(RootComponent);
 Torso->SetRelativeLocation(FVector(0,0,-12));
 Torso->SetRelativeScale3D(FVector(.53f,.40f,1.22f));
 UStaticMeshComponent* Head=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeroHead"));
 Head->SetupAttachment(RootComponent);
 Head->SetRelativeLocation(FVector(0,0,95));
 Head->SetRelativeScale3D(FVector(.36f,.36f,.39f));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
 if(Sphere.Succeeded()) { Torso->SetStaticMesh(Sphere.Object); Head->SetStaticMesh(Sphere.Object); }
 Torso->SetVisibility(!bHasOcean && !bHasCountess);
 Head->SetVisibility(!bHasOcean && !bHasCountess);
 // Temporary stylized female silhouette with long hair until a licensed skeletal mesh is imported.
 UStaticMeshComponent* Hair=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeroLongHair"));
 Hair->SetupAttachment(RootComponent);
 Hair->SetRelativeLocation(FVector(-19,0,68));
 Hair->SetRelativeScale3D(FVector(.35f,.48f,.80f));
 if(Sphere.Succeeded()) Hair->SetStaticMesh(Sphere.Object);
 Hair->SetVisibility(!bHasOcean && !bHasCountess);
 SwordVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SwordVisual"));
 SwordVisual->SetupAttachment(GetMesh(), TEXT("hand_rSocket"));
 SwordVisual->SetRelativeScale3D(FVector(.08f,.08f,1.0f));
 SwordVisual->SetVisibility(false);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 if(Cube.Succeeded()) SwordVisual->SetStaticMesh(Cube.Object);
 SwordVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 WeaponHead=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponHead"));
 WeaponGrip=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponGrip"));
 for(UStaticMeshComponent* Part : {WeaponHead,WeaponGrip})
 {
  Part->SetupAttachment(GetMesh(),TEXT("hand_rSocket"));
  Part->SetStaticMesh(Cube.Object);
  Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Part->SetVisibility(false);
 }
}
void ACryptHero::BeginPlay()
{
 Super::BeginPlay();
 for(const FName Candidate : {FName(TEXT("hand_rSocket")),FName(TEXT("hand_R")),FName(TEXT("RightHand"))})
 {
  if(!GetMesh()->DoesSocketExist(Candidate)) continue;
  GripBone=Candidate;
  break;
 }
 for(UStaticMeshComponent* Part : {SwordVisual,WeaponHead,WeaponGrip})
 {
  if(!GripBone.IsNone())
  {
   Part->AttachToComponent(GetMesh(),FAttachmentTransformRules::SnapToTargetNotIncludingScale,GripBone);
   Part->SetAbsolute(false,false,true);
  }
  else
   Part->AttachToComponent(GetRootComponent(),FAttachmentTransformRules::SnapToTargetNotIncludingScale);
 }
 if (OceanIdle && GetMesh() && GetMesh()->GetSkeletalMeshAsset())
 {
  GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
  GetMesh()->PlayAnimation(OceanIdle,true);
  ActiveOceanAnimation=OceanIdle;
  UE_LOG(LogTemp,Warning,TEXT("OCEAN_ANIM: BeginPlay idle started on mesh %s"),*GetNameSafe(GetMesh()->GetSkeletalMeshAsset()));
 }
 else UE_LOG(LogTemp,Error,TEXT("OCEAN_ANIM: missing mesh or idle; mesh=%s idle=%d"),*GetNameSafe(GetMesh()->GetSkeletalMeshAsset()),OceanIdle!=nullptr);
 SetActorLocation(FVector(-2900,0,150),false,nullptr,ETeleportType::TeleportPhysics);
 // Align the lowest point of the imported mesh with the collision capsule base.
 // Imported FBX origins are not guaranteed to match Unreal mannequin origins.
 if (GetMesh() && GetMesh()->GetSkeletalMeshAsset())
 {
  GetMesh()->UpdateBounds();
  const FBoxSphereBounds MeshBounds = GetMesh()->Bounds;
  const float MeshBottom = MeshBounds.Origin.Z - MeshBounds.BoxExtent.Z;
  const float CapsuleBottom = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
  const float Adjustment = CapsuleBottom - MeshBottom;
  if (FMath::IsFinite(Adjustment) && FMath::Abs(Adjustment) < 10000.f)
  {
   GetMesh()->AddWorldOffset(FVector(0,0,Adjustment));
  }
 }
}
void ACryptHero::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 AttackCooldown=FMath::Max(0.f,AttackCooldown-DeltaSeconds);
 UpdateStrike(DeltaSeconds);
 if(DialogueTime>0.f)
 {
  DialogueTime-=DeltaSeconds;
  if(DialogueTime<=0.f) Dialogue.Reset();
 }
 if (!OceanIdle || !GetMesh()) return;
 if (OceanActionTime > 0.f) OceanActionTime=FMath::Max(0.f,OceanActionTime-DeltaSeconds);
 UAnimSequence* Desired=OceanIdle;
 bool bLoop=true;
 if (OceanActionTime > 0.f && ActiveOceanAnimation)
 {
  Desired=ActiveOceanAnimation;
  bLoop=false;
 }
 else if (GetCharacterMovement()->IsFalling() && OceanJump)
 {
  Desired=OceanJump;
  bLoop=false;
 }
 else
 {
  const float Speed=GetVelocity().Size2D();
  if (Speed > 380.f && OceanRun) Desired=OceanRun;
  else if (Speed > 15.f && OceanWalk) Desired=OceanWalk;
 }
 if (GetMesh()->GetAnimationMode()!=EAnimationMode::AnimationSingleNode ||
     ActiveOceanAnimation!=Desired)
 {
  GetMesh()->PlayAnimation(Desired,bLoop);
  if(auto* Player=GetMesh()->GetSingleNodeInstance()) Player->SetPlayRate(1.f);
  ActiveOceanAnimation=Desired;
  GetMesh()->SetMorphTarget(TEXT("OceanRunLeftLeg"), Desired == OceanRun ? 1.f : 0.f);
 }
}
void ACryptHero::PlayOceanAction(UAnimSequence* Sequence,float Duration)
{
 if (!Sequence || !GetMesh()) return;
 ActiveOceanAnimation=Sequence;
 OceanActionTime=Duration;
 GetMesh()->PlayAnimation(Sequence,false);
 if(auto* Player=GetMesh()->GetSingleNodeInstance())
  Player->SetPlayRate(Sequence->GetPlayLength()/FMath::Max(Duration,.05f));
 GetMesh()->SetMorphTarget(TEXT("OceanRunLeftLeg"), 0.f);
}
void ACryptHero::SetupPlayerInputComponent(UInputComponent* Input)
{
 Super::SetupPlayerInputComponent(Input);
 Input->BindTouch(IE_Pressed,this,&ACryptHero::TouchPressed);
 Input->BindAxis("MoveForward",this,&ACryptHero::MoveForward);
 Input->BindAxis("MoveRight",this,&ACryptHero::MoveRight);
 Input->BindAxis("Turn",this,&ACryptHero::Turn);
 Input->BindAxis("LookUp",this,&ACryptHero::LookUp);
 Input->BindAction("Jump",IE_Pressed,this,&ACharacter::Jump);
 Input->BindAction("Attack",IE_Pressed,this,&ACryptHero::Attack);
 Input->BindAction("Spin",IE_Pressed,this,&ACryptHero::SpinAttack);
 Input->BindAction("Interact",IE_Pressed,this,&ACryptHero::Interact);
 Input->BindKey(EKeys::Tab,IE_Pressed,this,&ACryptHero::ToggleInventory);
 Input->BindKey(EKeys::R,IE_Pressed,this,&ACryptHero::CycleWeapon);
 Input->BindKey(EKeys::F5,IE_Pressed,this,&ACryptHero::SaveProgress);
 Input->BindKey(EKeys::F9,IE_Pressed,this,&ACryptHero::RestoreProgress);
}
void ACryptHero::MoveForward(float V) { if(FMath::Abs(V)>.01f) AddMovementInput(FRotationMatrix(FRotator(0,GetControlRotation().Yaw,0)).GetUnitAxis(EAxis::X),V); }
void ACryptHero::MoveRight(float V) { if(FMath::Abs(V)>.01f) AddMovementInput(FRotationMatrix(FRotator(0,GetControlRotation().Yaw,0)).GetUnitAxis(EAxis::Y),V); }
void ACryptHero::Turn(float V) { AddControllerYawInput(V); }
void ACryptHero::LookUp(float V) { AddControllerPitchInput(V); }
void ACryptHero::Attack()
{
 StartStrike(false);
}

void ACryptHero::SpinAttack()
{
 StartStrike(true);
}

void ACryptHero::StartStrike(bool bSpin)
{
 if(AttackCooldown>0.f || bInventoryOpen) return;
 bSpinStrike=bSpin;
 bStrikeResolved=false;
 StrikeWeapon=EquippedWeapon;
 StrikeAge=0.f;
 TrailSampleTime=0.f;
 StrikeTrail.Reset();
 StrikeDuration=bSpin?1.12f:(StrikeWeapon==ECryptWeapon::Axe?.90f:.78f);
 AttackCooldown=StrikeDuration+.05f;
 PlayOceanAction(bSpin?OceanSpin:OceanAttack,StrikeDuration);
}

void ACryptHero::ResolveStrike()
{
 const float Reach=WeaponReach()+(bSpinStrike?40.f:0.f);
 for(TActorIterator<ACryptMonster> It(GetWorld());It;++It)
 {
  if(It->GetRemainingHealth()<=0.f) continue;
  const FVector Offset=It->GetActorLocation()-GetActorLocation();
  if(Offset.SizeSquared()>=FMath::Square(Reach)) continue;
  if(bSpinStrike || FVector::DotProduct(GetActorForwardVector(),Offset.GetSafeNormal())>.25f)
   It->TakeHit(WeaponDamage());
 }
}

void ACryptHero::UpdateStrike(float DeltaSeconds)
{
 if(StrikeDuration<=0.f) return;
 StrikeAge+=DeltaSeconds;
 const float Phase=StrikeAge/StrikeDuration;
 if(!bStrikeResolved && Phase>=(bSpinStrike?.46f:.42f))
 {
  bStrikeResolved=true;
  ResolveStrike();
 }
 if(Phase>=1.f)
 {
  StrikeDuration=0.f;
  StrikeTrail.Reset();
  return;
 }
 if(Phase<.18f || Phase>.77f) return;
 TrailSampleTime+=DeltaSeconds;
 if(TrailSampleTime<.024f && !StrikeTrail.IsEmpty()) return;
 TrailSampleTime=0.f;
 const bool bArmed=StrikeWeapon!=ECryptWeapon::Unarmed;
 const FVector Wrist=GripBone.IsNone()
  ? GetActorTransform().TransformPosition(FVector(22,38,12))
  : GetMesh()->GetSocketLocation(GripBone);
 FVector End=Wrist;
 if(bArmed && EquippedWeapon==StrikeWeapon)
 {
  const UStaticMeshComponent* Blade=StrikeWeapon==ECryptWeapon::Sword?SwordVisual:WeaponHead;
  End=Blade->GetComponentLocation()+Blade->GetUpVector()*(50.f*Blade->GetComponentScale().Z);
 }
 else if(!GripBone.IsNone() && GetMesh()->DoesSocketExist(TEXT("forearm_R")))
 {
  const FVector Forearm=GetMesh()->GetSocketLocation(TEXT("forearm_R"));
  End=Wrist+(Wrist-Forearm).GetSafeNormal()*12.f;
 }
 StrikeTrail.Add(End);
 if(StrikeTrail.Num()>10) StrikeTrail.RemoveAt(0);
}

float ACryptHero::GetStrikeStrength() const
{
 if(StrikeDuration<=0.f) return 0.f;
 return 1.f-FMath::SmoothStep(.62f,1.f,StrikeAge/StrikeDuration);
}

ACryptTownActor* ACryptHero::GetInteractionTarget() const
{
 ACryptTownActor* Nearest=nullptr;
 double Closest=FMath::Square(325.0);
 for(TActorIterator<ACryptTownActor> It(GetWorld());It;++It)
 {
  if(It->QuestIndex<0 || It->QuestIndex>=CryptQuests::Count || It->bCollected) continue;
  if(!It->bQuestGiver && QuestState[It->QuestIndex]!=1) continue;
  const double Distance=FVector::DistSquared(GetActorLocation(),It->GetActorLocation());
  if(Distance<Closest)
  {
   Closest=Distance;
   Nearest=*It;
  }
 }
 return Nearest;
}

FString ACryptHero::GetInteractionLabel() const
{
 if(const ACryptTownActor* Target=GetInteractionTarget())
  return Target->bQuestGiver ? FString(CryptQuests::Data[Target->QuestIndex].Giver) : FString(TEXT("Inspect objective"));
 return {};
}

FString ACryptHero::GetQuestJournal() const
{
 FString Journal;
 for(int32 Index=0;Index<CryptQuests::Count;++Index)
 {
  const auto& Quest=CryptQuests::Data[Index];
  if(QuestState[Index]==1)
  {
   if(QuestProgress[Index]>=Quest.Target)
    Journal+=FString::Printf(TEXT("%s: return to %s\n"),Quest.Title,Quest.Giver);
   else
    Journal+=FString::Printf(TEXT("%s (%d/%d)\n"),Quest.Objective,QuestProgress[Index],Quest.Target);
  }
  else if(QuestState[Index]==2)
   Journal+=FString::Printf(TEXT("%s: complete\n"),Quest.Title);
 }
 return Journal.IsEmpty() ? FString(TEXT("Talk to a townsperson to begin.")) : Journal;
}

void ACryptHero::Interact()
{
 ACryptTownActor* Target=GetInteractionTarget();
 if(!Target)
 {
  Dialogue.Reset();
  return;
 }
 const int32 Index=Target->QuestIndex;
 const auto& Quest=CryptQuests::Data[Index];
 if(!Target->bQuestGiver)
 {
  QuestProgress[Index]=FMath::Min(QuestProgress[Index]+1,Quest.Target);
  Dialogue=FString::Printf(TEXT("%s (%d/%d)"),Quest.Objective,QuestProgress[Index],Quest.Target);
  Target->SetCollected(true);
 }
 else if(QuestState[Index]==0)
 {
  QuestState[Index]=1;
  Dialogue=FString::Printf(TEXT("%s: %s"),Quest.Giver,Quest.Offer);
 }
 else if(QuestState[Index]==1 && QuestProgress[Index]>=Quest.Target)
 {
  QuestState[Index]=2;
  AddTreasure(Quest.Treasure);
  ActiveWeaponMask|=1<<static_cast<int32>(Quest.Reward);
  Dialogue=FString::Printf(TEXT("%s: %s"),Quest.Giver,Quest.Thanks);
 }
 else
  Dialogue=FString::Printf(TEXT("%s: %s"),Quest.Giver,QuestState[Index]==2?Quest.Thanks:Quest.Objective);
 DialogueTime=7.f;
}

void ACryptHero::DefeatedRoadRaider()
{
 QuestProgress[0]=FMath::Min(QuestProgress[0]+1,CryptQuests::Data[0].Target);
}

bool ACryptHero::OwnsWeapon(ECryptWeapon Weapon) const
{
 return (ActiveWeaponMask & (1<<static_cast<int32>(Weapon)))!=0;
}

void ACryptHero::EquipWeapon(ECryptWeapon Weapon)
{
 if(!OwnsWeapon(Weapon) || StrikeDuration>0.f) return;
 EquippedWeapon=Weapon;
 bSwordEquipped=Weapon==ECryptWeapon::Sword;
 const bool bArmed=Weapon!=ECryptWeapon::Unarmed;
 for(UStaticMeshComponent* Part : {SwordVisual,WeaponHead,WeaponGrip})
  Part->SetVisibility(bArmed);
 if(!bArmed) return;
 const bool bSpear=Weapon==ECryptWeapon::Spear;
 const bool bAxe=Weapon==ECryptWeapon::Axe;
 const FVector Anchor=SwordVisual->GetAttachParent()==GetRootComponent()?FVector(24,44,0):FVector::ZeroVector;
 // Relative attachment offsets follow the hand; world scale keeps real weapon sizes.
 const float ParentScale=FMath::Max(.001f,float(SwordVisual->GetAttachParent()->GetComponentScale().Z));
 const auto Place=[&](UStaticMeshComponent* Part,FVector Offset,FVector Scale,FRotator Rotation=FRotator::ZeroRotator)
 {
  Part->SetRelativeLocationAndRotation(Anchor+Offset/ParentScale,Rotation);
  Part->SetWorldScale3D(Scale);
 };
 Place(SwordVisual,FVector(0,0,bSpear?27.f:38.f),bSpear?FVector(.035,.035,1.65):FVector(.045,bAxe?.055:.105,bAxe?.75:.72));
 Place(WeaponHead,FVector(0,bAxe?10.f:0.f,bSpear?116.f:(bAxe?72.f:8.f)),
  bSpear?FVector(.035,.10,.22):(bAxe?FVector(.05,.30,.23):FVector(.09,.31,.055)),bSpear?FRotator(0,0,25):FRotator::ZeroRotator);
 Place(WeaponGrip,FVector(0,0,-3),FVector(.075,.075,.22));
}

void ACryptHero::EquipSword(bool bEquip)
{
 if(bEquip) ActiveWeaponMask|=1<<static_cast<int32>(ECryptWeapon::Sword);
 EquipWeapon(bEquip?ECryptWeapon::Sword:ECryptWeapon::Unarmed);
}

void ACryptHero::CycleWeapon()
{
 for(int32 Offset=1;Offset<=4;++Offset)
 {
  const auto Weapon=static_cast<ECryptWeapon>((static_cast<int32>(EquippedWeapon)+Offset)%4);
  if(OwnsWeapon(Weapon))
  {
   EquipWeapon(Weapon);
   return;
  }
 }
}

float ACryptHero::WeaponDamage() const
{
 if(EquippedWeapon==ECryptWeapon::Axe) return 3.f;
 return EquippedWeapon==ECryptWeapon::Unarmed?1.f:2.f;
}

float ACryptHero::WeaponReach() const
{
 return EquippedWeapon==ECryptWeapon::Spear?365.f:260.f;
}

namespace
{
 const TCHAR* SaveSlot=TEXT("MindControl_Adventure");
}

void ACryptHero::SaveProgress()
{
 if(GetActorLocation().Z < -1000)
 {
  SaveStatus=TEXT("Move back onto solid ground before saving.");
  return;
 }
 auto* Save=Cast<UCryptSaveGame>(UGameplayStatics::CreateSaveGameObject(UCryptSaveGame::StaticClass()));
 if(!Save)
 {
  SaveStatus=TEXT("Could not create a save.");
  return;
 }
 Save->HeroLocation=GetActorLocation();
 Save->HeroRotation=GetActorRotation();
 Save->ViewRotation=GetControlRotation();
 Save->Treasure=TreasureCount;
 Save->Weapons=ActiveWeaponMask;
 Save->Equipped=static_cast<uint8>(EquippedWeapon);
 Save->QuestStates.Append(QuestState,CryptQuests::Count);
 Save->QuestCounts.Append(QuestProgress,CryptQuests::Count);
 for(TActorIterator<ACryptTownActor> It(GetWorld());It;++It)
  if(It->ObjectiveId!=INDEX_NONE && It->bCollected) Save->CollectedObjectives.Add(It->ObjectiveId);
 for(TActorIterator<ACryptMonster> It(GetWorld());It;++It)
 {
  if(It->SaveId==INDEX_NONE) continue;
  Save->EnemyHealth.Add(It->SaveId,It->GetRemainingHealth());
  Save->EnemyLocations.Add(It->SaveId,It->GetActorLocation());
 }
 SaveStatus=UGameplayStatics::SaveGameToSlot(Save,SaveSlot,0)?TEXT("Adventure saved."):TEXT("Could not write the save.");
}

void ACryptHero::RestoreProgress()
{
 const auto* Save=Cast<UCryptSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot,0));
 if(!Save)
 {
  SaveStatus=TEXT("No readable save was found.");
  return;
 }
 if(Save->Version!=1 || Save->QuestStates.Num()!=CryptQuests::Count ||
    Save->QuestCounts.Num()!=CryptQuests::Count || Save->HeroLocation.ContainsNaN() ||
    Save->HeroRotation.ContainsNaN() || Save->ViewRotation.ContainsNaN())
 {
  SaveStatus=TEXT("This save cannot be restored.");
  return;
 }
 if(!TeleportTo(Save->HeroLocation,Save->HeroRotation,false,true))
 {
  SaveStatus=TEXT("Could not return to the saved location.");
  return;
 }
 GetCharacterMovement()->StopMovementImmediately();
 StrikeDuration=0.f;
 StrikeTrail.Reset();
 AttackCooldown=0.f;
 OceanActionTime=0.f;
 if(AController* PC=GetController()) PC->SetControlRotation(Save->ViewRotation);
 TreasureCount=FMath::Max(0,Save->Treasure);
 ActiveWeaponMask=(Save->Weapons&15)|1;
 EquipWeapon(ECryptWeapon::Unarmed);
 if(Save->Equipped<=static_cast<uint8>(ECryptWeapon::Spear))
  EquipWeapon(static_cast<ECryptWeapon>(Save->Equipped));
 for(int32 Index=0;Index<CryptQuests::Count;++Index)
 {
  QuestState[Index]=FMath::Min<uint8>(Save->QuestStates[Index],2);
  QuestProgress[Index]=FMath::Clamp(Save->QuestCounts[Index],0,CryptQuests::Data[Index].Target);
 }
 for(TActorIterator<ACryptTownActor> It(GetWorld());It;++It)
  if(It->ObjectiveId!=INDEX_NONE) It->SetCollected(Save->CollectedObjectives.Contains(It->ObjectiveId));
 for(TActorIterator<ACryptMonster> It(GetWorld());It;++It)
 {
  const float* Health=Save->EnemyHealth.Find(It->SaveId);
  const FVector* Location=Save->EnemyLocations.Find(It->SaveId);
  if(Health && Location && FMath::IsFinite(*Health) && !Location->ContainsNaN())
   It->RestoreState(*Health,*Location);
 }
 Dialogue.Reset();
 SaveStatus=TEXT("Adventure restored.");
}

void ACryptHero::GetQuestMapMarkers(TArray<FCryptQuestMarker>& Markers) const
{
 for(TActorIterator<ACryptTownActor> It(GetWorld());It;++It)
 {
  const int32 Index=It->QuestIndex;
  if(Index<0 || Index>=CryptQuests::Count || QuestState[Index]!=1 || It->bCollected) continue;
  const bool bReturn=QuestProgress[Index]>=CryptQuests::Data[Index].Target;
  if(It->bQuestGiver==bReturn) Markers.Add({It->GetActorLocation(),Index});
 }
 if(QuestState[0]==1 && QuestProgress[0]<CryptQuests::Data[0].Target)
  for(TActorIterator<ACryptMonster> It(GetWorld());It;++It)
   if(It->bRoadRaider && It->GetRemainingHealth()>0.f) Markers.Add({It->GetActorLocation(),0});
}

int32 ACryptHero::GetPackedObjectiveCount(int32 Index) const
{
 return Index>=0 && Index<CryptQuests::Count && QuestState[Index]==1?QuestProgress[Index]:0;
}

void ACryptHero::TouchPressed(ETouchIndex::Type FingerIndex,FVector Location)
{
 if(!GEngine || !GEngine->GameViewport) return;
 FVector2D ViewSize;
 GEngine->GameViewport->GetViewportSize(ViewSize);
 if(ViewSize.X<=0 || ViewSize.Y<=0) return;
 const float W=ViewSize.X,H=ViewSize.Y;
 const FCryptHudLayout Layout(W,H);
 const float S=Layout.Scale;
 const FVector2D TouchPoint(Location.X,Location.Y);
 if(Layout.Hit(TouchPoint,Layout.Bag))
 {
  ToggleInventory();
  return;
 }
 if(bInventoryOpen)
 {
  if(Layout.HitPocket(TouchPoint,Layout.Save,Layout.UtilityRadius))
  {
   InspectedItem.Reset();
   SaveProgress();
   return;
  }
  if(Layout.HitPocket(TouchPoint,Layout.Restore,Layout.UtilityRadius))
  {
   InspectedItem.Reset();
   RestoreProgress();
   return;
  }
  if(Layout.HitPocket(TouchPoint,Layout.SupplyPocket(0),Layout.ItemRadius))
  {
   bSatchelJournal=!bSatchelJournal;
   return;
  }
  if(bSatchelJournal)
  {
   bSatchelJournal=false;
   return;
  }
  const TCHAR* Weapons[]={TEXT("Unarmed"),TEXT("Longsword"),TEXT("Battle axe"),TEXT("Spear")};
  for(int32 Index=0;Index<4;++Index)
  {
   if(!Layout.HitPocket(TouchPoint,Layout.WeaponPocket(Index),Layout.ItemRadius)) continue;
   const auto Weapon=static_cast<ECryptWeapon>(Index);
   if(OwnsWeapon(Weapon))
   {
    EquipWeapon(Weapon);
    InspectedItem=Weapons[Index];
   }
   return;
  }
  const TCHAR* Supplies[]={TEXT("Field journal"),TEXT("Coin purse"),TEXT("Valley herbs"),TEXT("Lost crest"),TEXT("Pay chest")};
  const int32 Counts[]={1,1,GetPackedObjectiveCount(1),GetPackedObjectiveCount(2),GetPackedObjectiveCount(4)};
  for(int32 Index=1;Index<5;++Index)
   if(Counts[Index]>0 && Layout.HitPocket(TouchPoint,Layout.SupplyPocket(Index),Layout.ItemRadius))
    InspectedItem=Supplies[Index];
  return;
 }
 if(Layout.Hit(TouchPoint,Layout.Interact) && !GetInteractionLabel().IsEmpty())
 {
  Interact();
  return;
 }
 // Right virtual thumb pad: a tap in its inner circle triggers jump.
 // Keep the outer ring available for normal right-stick camera movement.
 const FVector2D RightPadCenter(W*.875f,H*.855f);
 const float RightPadInnerRadius=66.f*S;
 if(FVector2D::Distance(TouchPoint,RightPadCenter)<=RightPadInnerRadius)
 {
  Jump();
  UE_LOG(LogTemp,Log,TEXT("MOBILE_RIGHT_PAD: center pressed, jump requested"));
  return;
 }
 if(Layout.Hit(TouchPoint,Layout.Jump)) Jump();
 else if(Layout.Hit(TouchPoint,Layout.Attack)) Attack();
 else if(Layout.Hit(TouchPoint,Layout.Spin)) SpinAttack();
}
