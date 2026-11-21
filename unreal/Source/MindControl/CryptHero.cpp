#include "CryptHero.h"
#include "Misc/ConfigCacheIni.h"
#include "CryptMonster.h"
#include "CryptDungeons.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
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
namespace
{
 int32 GearGrade(int32 Id) { return Id>=0?1+Id/256:0; }
 int32 CompletedDungeonCount(uint32 Mask)
 {
  int32 Count=0;
  for(;Mask;Mask&=Mask-1) ++Count;
  return Count;
 }
 bool CombatVisible(const AActor* From,const AActor* To)
 {
  FCollisionQueryParams Query;
  Query.AddIgnoredActor(From);
  Query.AddIgnoredActor(To);
  return !From->GetWorld()->LineTraceTestByChannel(From->GetActorLocation(),To->GetActorLocation(),ECC_Visibility,Query);
 }
}

ACryptHero::ACryptHero()
{
 PrimaryActorTick.bCanEverTick = true;
 GetCharacterMovement()->MaxWalkSpeed = 520;
 GetCharacterMovement()->JumpZVelocity = 650;
 GetCharacterMovement()->AirControl = .45f;
 // Keep the orbit camera independent while turning the hero toward travel.
 bUseControllerRotationYaw = false;
 GetCharacterMovement()->bOrientRotationToMovement = true;
 GetCharacterMovement()->RotationRate = FRotator(0.f,680.f,0.f);
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
  static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkDiagonal(TEXT("/Game/Characters/OceanSerenity/OceanSerenityOcean_WalkDiagonal.OceanSerenityOcean_WalkDiagonal"));
  static ConstructorHelpers::FObjectFinder<UAnimSequence> RunDiagonal(TEXT("/Game/Characters/OceanSerenity/OceanSerenityOcean_RunDiagonal.OceanSerenityOcean_RunDiagonal"));
  OceanIdle=Idle.Object; OceanWalk=Walk.Object; OceanRun=Run.Object;
  OceanWalkDiagonal=WalkDiagonal.Object; OceanRunDiagonal=RunDiagonal.Object;
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
 int32 StoredAction=0;
 if(GConfig && GConfig->GetInt(TEXT("MindControl.Controls"),TEXT("RightThumbAction"),StoredAction,GGameUserSettingsIni) &&
    StoredAction>=0 && StoredAction<int32(ECryptThumbAction::Count))
  ThumbAction=static_cast<ECryptThumbAction>(StoredAction);
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
 UpdateChart();
 UpdateGearVisuals();
 AttackCooldown=FMath::Max(0.f,AttackCooldown-DeltaSeconds);
 HurtCooldown=FMath::Max(0.f,HurtCooldown-DeltaSeconds);
 SpellCooldown=FMath::Max(0.f,SpellCooldown-DeltaSeconds);
 RecoveryDelay=FMath::Max(0.f,RecoveryDelay-DeltaSeconds);
 if(RecoveryDelay<=0.f) Health=FMath::Min(100.f,Health+DeltaSeconds*4.f);
 if(AttackCooldown<=0.f) Stamina=FMath::Min(100.f,Stamina+DeltaSeconds*19.f);
 if(SpellCooldown<=0.f) Mana=FMath::Min(75.f,Mana+DeltaSeconds*7.f);
 UpdateStrike(DeltaSeconds);
 if(DialogueTime>0.f)
 {
  DialogueTime-=DeltaSeconds;
  if(DialogueTime<=0.f) Dialogue.Reset();
 }
 if (!OceanIdle || !GetMesh()) return;
 if (OceanActionTime > 0.f) OceanActionTime=FMath::Max(0.f,OceanActionTime-DeltaSeconds);
 auto IsGait=[this](const UAnimSequence* Sequence)
 {
  return Sequence && (Sequence==OceanWalk || Sequence==OceanRun ||
                      Sequence==OceanWalkDiagonal || Sequence==OceanRunDiagonal);
 };
 UAnimSequence* Desired=OceanIdle;
 const FVector Velocity=GetVelocity();
 const float Speed=Velocity.Size2D();
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
  if (Speed > 15.f)
  {
   const FRotator CameraYaw(0.f,GetControlRotation().Yaw,0.f);
   const FVector LocalVelocity=CameraYaw.UnrotateVector(Velocity);
   const float DiagonalAmount=FMath::Min(FMath::Abs(LocalVelocity.X),FMath::Abs(LocalVelocity.Y))/Speed;
   const bool bWasDiagonal=ActiveOceanAnimation &&
     (ActiveOceanAnimation==OceanWalkDiagonal || ActiveOceanAnimation==OceanRunDiagonal);
   const bool bDiagonal=DiagonalAmount>(bWasDiagonal?.28f:.40f);
   const bool bWasRunning=ActiveOceanAnimation &&
     (ActiveOceanAnimation==OceanRun || ActiveOceanAnimation==OceanRunDiagonal);
   if (Speed>(bWasRunning?165.f:195.f) && OceanRun)
    Desired=bDiagonal && OceanRunDiagonal?OceanRunDiagonal:OceanRun;
   else if (OceanWalk)
    Desired=bDiagonal && OceanWalkDiagonal?OceanWalkDiagonal:OceanWalk;
  }
 }
 if (GetMesh()->GetAnimationMode()!=EAnimationMode::AnimationSingleNode ||
     ActiveOceanAnimation!=Desired)
 {
  float StridePhase=0.f;
  if (IsGait(ActiveOceanAnimation) && IsGait(Desired))
   if (auto* Player=GetMesh()->GetSingleNodeInstance())
    StridePhase=Player->GetCurrentTime()/FMath::Max(.01f,ActiveOceanAnimation->GetPlayLength());
  GetMesh()->PlayAnimation(Desired,bLoop);
  if (auto* Player=GetMesh()->GetSingleNodeInstance())
  {
   Player->SetPlayRate(1.f);
   Player->SetPosition(StridePhase*Desired->GetPlayLength(),false);
  }
  ActiveOceanAnimation=Desired;
  GetMesh()->SetMorphTarget(TEXT("OceanRunLeftLeg"), Desired==OceanRun || Desired==OceanRunDiagonal ? 1.f : 0.f);
 }
 if (bLoop)
 {
  const bool bRunning=Desired && (Desired==OceanRun || Desired==OceanRunDiagonal);
  const float ReferenceSpeed=bRunning?290.f:105.f;
  const float Rate=IsGait(Desired)?FMath::Clamp(Speed/ReferenceSpeed,.50f,1.85f):1.f;
  if (auto* Player=GetMesh()->GetSingleNodeInstance()) Player->SetPlayRate(Rate);
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
 Input->BindTouch(IE_Released,this,&ACryptHero::TouchReleased);
 Input->BindAxis("MoveForward",this,&ACryptHero::MoveForward);
 Input->BindAxis("MoveRight",this,&ACryptHero::MoveRight);
 Input->BindAxis("Turn",this,&ACryptHero::Turn);
 Input->BindAxis("LookUp",this,&ACryptHero::LookUp);
 Input->BindAction("Jump",IE_Pressed,this,&ACharacter::Jump);
 Input->BindAction("Attack",IE_Pressed,this,&ACryptHero::Attack);
 Input->BindAction("Spin",IE_Pressed,this,&ACryptHero::SpinAttack);
 Input->BindAction("Interact",IE_Pressed,this,&ACryptHero::Interact);
 Input->BindKey(EKeys::Tab,IE_Pressed,this,&ACryptHero::ToggleInventory);
 Input->BindKey(EKeys::One,IE_Pressed,this,&ACryptHero::CastFire);
 Input->BindKey(EKeys::Two,IE_Pressed,this,&ACryptHero::CastStorm);
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
 if(EquippedWeapon==ECryptWeapon::Bow && !bSpin)
 {
  FireBow();
  return;
 }
 const float Cost=bSpin?25.f:(EquippedWeapon==ECryptWeapon::Axe?16.f:10.f);
 if(Stamina<Cost) return;
 Stamina-=Cost;
 bSpinStrike=bSpin;
 bStrikeResolved=false;
 StrikeWeapon=EquippedWeapon;
 StrikeAge=0.f;
 TrailSampleTime=0.f;
 StrikeTrail.Reset();
 StrikeDuration=bSpin?1.12f:(StrikeWeapon==ECryptWeapon::Axe?.90f:.78f);
 AttackCooldown=StrikeDuration+FMath::Max(.04f,.28f-GetDexterity()*.010f);
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
  if((bSpinStrike || FVector::DotProduct(GetActorForwardVector(),Offset.GetSafeNormal())>.25f) && CombatVisible(this,*It))
   It->TakeHit(WeaponDamage()*(bSpinStrike?1.20f:1.f),GetDexterity(),ECryptDamage::Physical);
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
 if(FindSecret()) return TEXT("Inspect loose stone");
 if(FindMerchant()!=INDEX_NONE) return TEXT("Trade with merchant");
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
 if(AActor* Secret=FindSecret())
 {
  OpenSecrets.Add(FCString::Atoi(*Secret->Tags[1].ToString()));
  Secret->SetActorHiddenInGame(true);
  Secret->SetActorEnableCollision(false);
  AppendEvent(TEXT("A hidden passage opens."));
  return;
 }
 const int32 Shop=FindMerchant();
 if(Shop!=INDEX_NONE)
 {
  ToggleInventory();
  bInventoryOpen=true;
  bGearPage=true;
  MerchantId=Shop;
  StockIndex=0;
  return;
 }
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
  AddExperience(75);
  AddTreasure(Quest.Treasure);
  ActiveWeaponMask|=1<<static_cast<int32>(Quest.Reward);
  Dialogue=FString::Printf(TEXT("%s: %s"),Quest.Giver,Quest.Thanks);
 }
 else
  Dialogue=FString::Printf(TEXT("%s: %s"),Quest.Giver,QuestState[Index]==2?Quest.Thanks:Quest.Objective);
 AppendEvent(Dialogue);
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
 LootWeapon=INDEX_NONE;
 EquippedWeapon=Weapon;
 bSwordEquipped=Weapon==ECryptWeapon::Sword;
 const bool bArmed=Weapon!=ECryptWeapon::Unarmed && Weapon!=ECryptWeapon::Bow;
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
 for(int32 Offset=1;Offset<=5;++Offset)
 {
  const auto Weapon=static_cast<ECryptWeapon>((static_cast<int32>(EquippedWeapon)+Offset)%5);
  if(OwnsWeapon(Weapon))
  {
   EquipWeapon(Weapon);
   return;
  }
 }
}

float ACryptHero::WeaponDamage() const
{
 const float Base=EquippedWeapon==ECryptWeapon::Axe?29.f:EquippedWeapon==ECryptWeapon::Spear?21.f:
  EquippedWeapon==ECryptWeapon::Unarmed?13.f:24.f;
 return (EquippedWeapon==ECryptWeapon::Bow?22.f:Base)+GearGrade(LootWeapon)*1.4f+(Level-1)*.6f;
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
 Save->Version=5;
 Save->ChartTiles=ChartTiles;
 Save->OpenSecrets=OpenSecrets;
 Save->EventLog=EventLog;
 Save->Relics=Relics;
 Save->HeroLevel=Level;
 Save->HeroExperience=Experience;
 Save->ArrowType=ArrowType;
 Save->Arrows.Append(Arrows,3);
 Save->LootWeapon=LootWeapon;
 Save->LootArmor=LootArmor;
 Save->HeroHealth=Health;
 Save->HeroStamina=Stamina;
 Save->HeroMana=Mana;
 Save->ClearedDungeons=int32(ClearedDungeons);
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
 if((Save->Version<1 || Save->Version>5) || Save->QuestStates.Num()!=CryptQuests::Count ||
    Save->QuestCounts.Num()!=CryptQuests::Count || Save->HeroLocation.ContainsNaN() ||
    Save->HeroRotation.ContainsNaN() || Save->ViewRotation.ContainsNaN())
 {
  SaveStatus=TEXT("This save cannot be restored.");
  return;
 }
 if(!TeleportTo(Save->Version<5?CryptDungeons::MigrateLocation(Save->HeroLocation,Save->Version):Save->HeroLocation,Save->HeroRotation,false,true))
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
 ChartTiles=Save->ChartTiles;
 OpenSecrets=Save->OpenSecrets;
 EventLog=Save->EventLog;
 Relics=Save->Relics;
 LogOffset=0;
 ChartIndex=INDEX_NONE;
 for(TActorIterator<AStaticMeshActor> It(GetWorld());It;++It)
  if(It->ActorHasTag(TEXT("DungeonSecret")) && It->Tags.Num()>1)
  {
   const bool bOpened=OpenSecrets.Contains(FCString::Atoi(*It->Tags[1].ToString()));
   It->SetActorHiddenInGame(bOpened);
   It->SetActorEnableCollision(!bOpened);
  }
 Health=Save->Version>=2 && FMath::IsFinite(Save->HeroHealth)?FMath::Clamp(Save->HeroHealth,1.f,100.f):100.f;
 Stamina=Save->Version>=2 && FMath::IsFinite(Save->HeroStamina)?FMath::Clamp(Save->HeroStamina,0.f,100.f):100.f;
 Mana=Save->Version>=2 && FMath::IsFinite(Save->HeroMana)?FMath::Clamp(Save->HeroMana,0.f,75.f):75.f;
 ClearedDungeons=Save->Version>=2?uint32(Save->ClearedDungeons)&1023u:0u;
 SpellCooldown=0.f;
 HurtCooldown=1.f;
 RecoveryDelay=0.f;
 Level=FMath::Clamp(Save->HeroLevel,1,100);
 Experience=FMath::Clamp(Save->HeroExperience,0,ExperienceNeeded()-1);
 ArrowType=FMath::Clamp(Save->ArrowType,0,2);
 if(Save->Arrows.Num()==3)
  for(int32 Index=0;Index<3;++Index) Arrows[Index]=FMath::Clamp(Save->Arrows[Index],0,999);
 else
 {
  Arrows[0]=30;
  Arrows[1]=10;
  Arrows[2]=10;
 }
 ActiveWeaponMask=(Save->Weapons&31)|17;
 EquipWeapon(ECryptWeapon::Unarmed);
 if(Save->Equipped<=static_cast<uint8>(ECryptWeapon::Bow))
  EquipWeapon(static_cast<ECryptWeapon>(Save->Equipped));
 LootWeapon=Relics.Contains(Save->LootWeapon)?Save->LootWeapon:INDEX_NONE;
 LootArmor=Relics.Contains(Save->LootArmor)?Save->LootArmor:INDEX_NONE;
 GearIndex=0;
 StockIndex=0;
 for(int32 Index=0;Index<CryptQuests::Count;++Index)
 {
  QuestState[Index]=FMath::Min<uint8>(Save->QuestStates[Index],2);
  QuestProgress[Index]=FMath::Clamp(Save->QuestCounts[Index],0,CryptQuests::Data[Index].Target);
 }
 for(TActorIterator<ACryptTownActor> It(GetWorld());It;++It)
  if(It->ObjectiveId!=INDEX_NONE) It->SetCollected(Save->CollectedObjectives.Contains(It->ObjectiveId));
 for(TActorIterator<ACryptMonster> It(GetWorld());It;++It)
 {
  const float* SavedEnemyHealth=Save->EnemyHealth.Find(It->SaveId);
  const FVector* Location=Save->EnemyLocations.Find(It->SaveId);
  if(SavedEnemyHealth && Location && FMath::IsFinite(*SavedEnemyHealth) && !Location->ContainsNaN())
   It->RestoreState(Save->Version==1?(*SavedEnemyHealth/3.f)*It->Stats().Health:*SavedEnemyHealth,Save->Version<5 && It->SaveId>=1000 ? It->GetActorLocation() : *Location);
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
 const FVector2D TouchPoint(Location.X,Location.Y);
 if(Layout.Hit(TouchPoint,Layout.Bag))
 {
  ToggleInventory();
  return;
 }
 if(bInventoryOpen)
 {
  if(Layout.HitGear(TouchPoint,Layout.GearTab()))
  {
   bGearPage=!bGearPage;
   bSatchelJournal=false;
   return;
  }
  if(bGearPage && Layout.HitGear(TouchPoint,Layout.InSatchel(.19f,.79f)))
  {
   EquipWeapon(ECryptWeapon::Bow);
   InspectedItem=TEXT("Bow equipped.");
   return;
  }
  if(bGearPage && Layout.HitGear(TouchPoint,Layout.InSatchel(.39f,.79f)))
  {
   ArrowType=(ArrowType+1)%3;
   return;
  }
  if(bGearPage && MerchantId!=INDEX_NONE && Layout.HitGear(TouchPoint,Layout.InSatchel(.79f,.79f)))
  {
   BuyArrows();
   return;
  }
  if(bGearPage && !Layout.HitPocket(TouchPoint,Layout.Save,Layout.UtilityRadius) &&
     !Layout.HitPocket(TouchPoint,Layout.Restore,Layout.UtilityRadius))
  {
   for(int32 Control=0;Control<4;++Control)
    if(Layout.HitGear(TouchPoint,Layout.GearButton(Control)))
    {
     if(Control==0 && !Relics.IsEmpty()) GearIndex=(GearIndex+Relics.Num()-1)%Relics.Num();
     if(Control==1 && !Relics.IsEmpty()) GearIndex=(GearIndex+1)%Relics.Num();
     if(Control==2) UseGear();
     if(Control==3 && MerchantId!=INDEX_NONE) SellGear();
     return;
    }
   if(MerchantId!=INDEX_NONE)
   {
    if(Layout.HitGear(TouchPoint,Layout.InSatchel(.59f,.51f))) StockIndex=(StockIndex+1)%8;
    if(Layout.HitGear(TouchPoint,Layout.InSatchel(.79f,.51f))) BuyGear();
   }
   return;
  }
  if(!bSatchelJournal)
   for(int32 Index=0;Index<int32(ECryptThumbAction::Count);++Index)
    if(Layout.HitThumbAction(TouchPoint,Index))
    {
     SetThumbAction(static_cast<ECryptThumbAction>(Index));
     return;
    }
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
 if(Layout.HitPocket(TouchPoint,Layout.LogOlder,13.f*Layout.Scale))
 {
  ScrollLog(1);
  return;
 }
 if(Layout.HitPocket(TouchPoint,Layout.LogNewer,13.f*Layout.Scale))
 {
  ScrollLog(-1);
  return;
 }
 if(Layout.Hit(TouchPoint,Layout.Interact) && !GetInteractionLabel().IsEmpty())
 {
  Interact();
  return;
 }
 if(FVector2D::Distance(TouchPoint,Layout.RightThumbCenter)<=Layout.RightThumbRadius)
 {
  ThumbFinger=int32(FingerIndex);
  ThumbStart=TouchPoint;
  ThumbStartTime=GetWorld()->GetRealTimeSeconds();
  return;
 }
 if(Layout.Hit(TouchPoint,Layout.Jump)) Jump();
 else if(Layout.Hit(TouchPoint,Layout.Attack)) Attack();
 else if(Layout.Hit(TouchPoint,Layout.Spin)) SpinAttack();
}



void ACryptHero::SetThumbAction(ECryptThumbAction Action)
{
 ThumbAction=Action;
 InspectedItem=FString::Printf(TEXT("Right thumb: %s"),CryptThumbActionLabels[int32(Action)]);
 if(GConfig)
 {
  GConfig->SetInt(TEXT("MindControl.Controls"),TEXT("RightThumbAction"),int32(Action),GGameUserSettingsIni);
  GConfig->Flush(false,GGameUserSettingsIni);
 }
}

void ACryptHero::TouchMoved(ETouchIndex::Type FingerIndex,FVector Location)
{
 if(ThumbFinger!=int32(FingerIndex) || !GEngine || !GEngine->GameViewport) return;
 FVector2D Size;
 GEngine->GameViewport->GetViewportSize(Size);
 const FCryptHudLayout Layout(Size.X,Size.Y);
 if(FVector2D::Distance(ThumbStart,FVector2D(Location.X,Location.Y))>24.f*Layout.Scale)
  ThumbFinger=INDEX_NONE;
}

void ACryptHero::TouchReleased(ETouchIndex::Type FingerIndex,FVector Location)
{
 if(ThumbFinger!=int32(FingerIndex)) return;
 ThumbFinger=INDEX_NONE;
 if(bInventoryOpen || !GEngine || !GEngine->GameViewport ||
    GetWorld()->GetRealTimeSeconds()-ThumbStartTime>.38f) return;
 FVector2D Size;
 GEngine->GameViewport->GetViewportSize(Size);
 const FCryptHudLayout Layout(Size.X,Size.Y);
 if(FVector2D::Distance(ThumbStart,FVector2D(Location.X,Location.Y))>24.f*Layout.Scale) return;
 switch(ThumbAction)
 {
  case ECryptThumbAction::Jump: Jump(); break;
  case ECryptThumbAction::Attack: Attack(); break;
  case ECryptThumbAction::Spin: SpinAttack(); break;
  case ECryptThumbAction::Fire: CastFire(); break;
  case ECryptThumbAction::Storm: CastStorm(); break;
  default: break;
 }
}

float ACryptHero::GetArmorRating() const
{
 return 4.f+CompletedDungeonCount(ClearedDungeons)*1.5f+(Level-1)*.55f+GearGrade(LootArmor)*1.25f;
}

float ACryptHero::GetDexterity() const
{
 return 18.f+CompletedDungeonCount(ClearedDungeons)*.8f+(Level-1)*.5f;
}

FString ACryptHero::GetCombatTarget() const
{
 const int32 Dungeon=CryptDungeons::Current(GetActorLocation());
 const ACryptMonster* Target=nullptr;
 double Nearest=FMath::Square(780.);
 int32 Remaining=0;
 for(TActorIterator<ACryptMonster> It(GetWorld());It;++It)
 {
  if(It->GetRemainingHealth()<=0.f) continue;
  if(Dungeon!=INDEX_NONE && It->GetDungeon()==Dungeon) ++Remaining;
  const double Distance=FVector::DistSquared(GetActorLocation(),It->GetActorLocation());
  if(Distance<Nearest && CombatVisible(this,*It))
  {
   Target=*It;
   Nearest=Distance;
  }
 }
 FString Text;
 if(Dungeon!=INDEX_NONE)
  Text=FString::Printf(TEXT("%s | %d remaining"),CryptDungeons::Chambers[Dungeon].Name,Remaining);
 if(Target)
 {
  const auto& Profile=Target->Stats();
  const TCHAR* Weakness=Profile.Fire>1.2f || Profile.Storm>1.2f
   ? (Profile.Fire>Profile.Storm?TEXT(" | Weak: Fire"):TEXT(" | Weak: Storm")) : TEXT("");
  if(!Text.IsEmpty()) Text+=TEXT("\n");
  Text+=FString::Printf(TEXT("%s%s  %d/%d HP  |  Armor %d%s"),Target->IsBoss()?TEXT("Boss: "):TEXT(""),Profile.Name,
     FMath::CeilToInt(Target->GetRemainingHealth()),int32(Profile.Health),int32(Profile.Armor),Weakness);
 }
 return Text;
}

void ACryptHero::ReceiveCombatHit(float Rating,float Dexterity,ECryptDamage Element)
{
 if(HurtCooldown>0.f || FMath::FRand()>CryptCombat::HitChance(Dexterity,GetDexterity())) return;
 HurtCooldown=.33f;
 Health=FMath::Max(0.f,Health-CryptCombat::Damage(Rating,GetArmorRating(),Element));
 RecoveryDelay=5.f;
 AppendEvent(FString::Printf(TEXT("You take %d damage."),FMath::CeilToInt(CryptCombat::Damage(Rating,GetArmorRating(),Element))));
 if(Health>0.f) return;
 const int32 Dungeon=CryptDungeons::Current(GetActorLocation());
 FVector Safe=FVector(-2450,175,110);
 if(Dungeon!=INDEX_NONE) Safe=CryptDungeons::SurfaceArrival(Dungeon);
 SetActorLocation(Safe,false,nullptr,ETeleportType::TeleportPhysics);
 GetCharacterMovement()->StopMovementImmediately();
 Health=100.f;
 Stamina=100.f;
 Mana=75.f;
 StrikeDuration=0.f;
 AttackCooldown=0.f;
 SpellCooldown=0.f;
 OceanActionTime=0.f;
 StrikeTrail.Reset();
 HurtCooldown=2.f;
 Dialogue=TEXT("You were defeated and retreated to safety.");
 AppendEvent(Dialogue);
 DialogueTime=4.f;
}

void ACryptHero::CheckDungeonClear(int32 Dungeon)
{
 if(Dungeon<0 || Dungeon>=CryptDungeons::Count || (ClearedDungeons & (1u<<Dungeon))) return;
 for(TActorIterator<ACryptMonster> It(GetWorld());It;++It)
  if(It->GetDungeon()==Dungeon && It->GetRemainingHealth()>0.f) return;
 ClearedDungeons|=1u<<Dungeon;
 AddExperience(220);
 AddTreasure(10+Dungeon*2);
 Health=100.f;
 Stamina=100.f;
 Mana=75.f;
 Dialogue=FString::Printf(TEXT("%s cleared. Armor and dexterity increased."),CryptDungeons::Chambers[Dungeon].Name);
 AppendEvent(Dialogue);
 DialogueTime=6.f;
}

void ACryptHero::ShowSpell(FVector From,FVector To,ECryptDamage Element,FLinearColor Color)
{
 auto* EffectMesh=LoadObject<UStaticMesh>(nullptr,Element==ECryptDamage::Fire
  ?TEXT("/Engine/BasicShapes/Sphere.Sphere"):TEXT("/Engine/BasicShapes/Cube.Cube"));
 auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
 const auto Part=[&](FVector Position,FVector Scale,FRotator Rotation,float Duration)
 {
  auto* Effect=GetWorld()->SpawnActor<AStaticMeshActor>(Position,Rotation);
  if(!Effect) return;
  auto* Shape=Effect->GetStaticMeshComponent();
  Shape->SetMobility(EComponentMobility::Movable);
  Shape->SetStaticMesh(EffectMesh);
  Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Shape->SetCastShadow(false);
  if(Base)
  {
   auto* Tint=UMaterialInstanceDynamic::Create(Base,Effect);
   Tint->SetVectorParameterValue(TEXT("Color"),Color);
   Shape->SetMaterial(0,Tint);
  }
  Effect->SetActorScale3D(Scale);
  Effect->SetLifeSpan(Duration);
 };
 if(Element==ECryptDamage::Fire)
 {
  for(int32 N=1;N<=5;++N)
   Part(FMath::Lerp(From,To,N*.2),FVector(.22+N*.08),FRotator::ZeroRotator,.12f+N*.045f);
  return;
 }
 const FVector Side=FVector::CrossProduct((To-From).GetSafeNormal(),FVector::UpVector);
 FVector Start=From;
 for(int32 N=1;N<=6;++N)
 {
  const FVector End=FMath::Lerp(From,To,N/6.)+Side*(N==6?0:(N%2?-21:21));
  const FVector Segment=End-Start;
  Part((Start+End)*.5,FVector(Segment.Size()/100.,.08,.08),Segment.Rotation(),.23f);
  Start=End;
 }
}

void ACryptHero::CastSpell(ECryptDamage Element)
{
 if(bInventoryOpen || SpellCooldown>0.f || AttackCooldown>0.f) return;
 const bool bFire=Element==ECryptDamage::Fire;
 const float Cost=bFire?19.f:28.f;
 if(Mana<Cost)
 {
  Dialogue=TEXT("Not enough mana.");
  DialogueTime=1.25f;
  return;
 }
 const FVector Start=GetActorLocation()+FVector(0,0,30);
 ACryptMonster* Target=nullptr;
 double Nearest=FMath::Square(bFire?770.:950.);
 for(TActorIterator<ACryptMonster> It(GetWorld());It;++It)
 {
  if(It->GetRemainingHealth()<=0.f) continue;
  const FVector Offset=It->GetActorLocation()-Start;
  const double Range=Offset.SizeSquared();
  if(Range<Nearest && FVector::DotProduct(Offset.GetSafeNormal(),GetActorForwardVector())>.39f &&
     CombatVisible(this,*It))
  {
   Target=*It;
   Nearest=Range;
  }
 }
 if(!Target)
 {
  Dialogue=TEXT("Face an enemy within spell range.");
  DialogueTime=1.25f;
  return;
 }
 Mana-=Cost;
 SpellCooldown=1.05f;
 AttackCooldown=.42f;
 PlayOceanAction(OceanAttack,.42f);
 const FLinearColor Color=bFire?FLinearColor(1.f,.25f,.045f):FLinearColor(.35f,.72f,1.f);
 const FVector Hit=Target->GetActorLocation()+FVector(0,0,22);
 ShowSpell(Start,Hit,Element,Color);
 Target->TakeHit(bFire?34.f:28.f,GetDexterity(),Element);
 if(bFire) return;
 ACryptMonster* Chain=nullptr;
 Nearest=FMath::Square(430.);
 for(TActorIterator<ACryptMonster> It(GetWorld());It;++It)
 {
  if(*It==Target || It->GetRemainingHealth()<=0.f) continue;
  const double Range=FVector::DistSquared(It->GetActorLocation(),Hit);
  if(Range<Nearest && CombatVisible(Target,*It))
  {
   Chain=*It;
   Nearest=Range;
  }
 }
 if(Chain)
 {
  ShowSpell(Hit,Chain->GetActorLocation(),Element,Color);
  Chain->TakeHit(17.f,GetDexterity(),Element);
 }
}

namespace
{
 constexpr int32 RelicShapes=16;
 constexpr int32 RelicFinishes=16;
 constexpr int32 RelicGrades=16;
 constexpr int32 RelicCount=RelicShapes*RelicFinishes*RelicGrades;
 const TCHAR* RelicNames[]={
  TEXT("Knife"),TEXT("Shortsword"),TEXT("Longsword"),TEXT("Ritual blade"),
  TEXT("Shield"),TEXT("Armguard"),TEXT("Chestguard"),TEXT("Ward plate"),
  TEXT("Flask"),TEXT("Tonic"),TEXT("Elixir"),TEXT("Draught"),
  TEXT("Shard"),TEXT("Crystal"),TEXT("Geode"),TEXT("Crownstone")
 };
 const TCHAR* FinishNames[]={
  TEXT("Iron"),TEXT("Silver"),TEXT("Bronze"),TEXT("Gold"),TEXT("Jade"),TEXT("Ruby"),
  TEXT("Sapphire"),TEXT("Amethyst"),TEXT("Onyx"),TEXT("Pearl"),TEXT("Copper"),TEXT("Amber"),
  TEXT("Quartz"),TEXT("Obsidian"),TEXT("Moonstone"),TEXT("Verdigris")
 };

 FString RelicName(int32 Id)
 {
  if(Id<0 || Id>=RelicCount) return FString();
  return FString::Printf(TEXT("%s %s +%d"),FinishNames[(Id/RelicShapes)%RelicFinishes],
     RelicNames[Id%RelicShapes],1+Id/(RelicShapes*RelicFinishes));
 }
}

void ACryptHero::AppendEvent(const FString& Message)
{
 if(Message.IsEmpty()) return;
 EventLog.Add(Message);
 if(EventLog.Num()>120) EventLog.RemoveAt(0);
 if(LogOffset>0) ScrollLog(1);
}

void ACryptHero::ScrollLog(int32 Delta)
{
 LogOffset=FMath::Clamp(LogOffset+Delta,0,FMath::Max(0,EventLog.Num()-3));
}

void ACryptHero::UpdateChart()
{
 const int32 Here=CryptDungeons::ChartDungeon(GetActorLocation());
 if(Here!=ChartIndex)
 {
  if(Here!=INDEX_NONE) AppendEvent(FString::Printf(TEXT("Entered %s."),CryptDungeons::Chambers[Here].Name));
  else if(ChartIndex!=INDEX_NONE) AppendEvent(TEXT("Returned to the surface."));
  ChartIndex=Here;
 }
 if(Here!=INDEX_NONE)
 {
  const int32 Tile=CryptDungeons::ChartTile(Here,GetActorLocation());
  if(Tile!=INDEX_NONE)
   ChartTiles.Add(Here*CryptDungeons::ChartColumns*CryptDungeons::ChartRows+Tile);
 }
}

AActor* ACryptHero::FindSecret() const
{
 if(ChartIndex==INDEX_NONE) return nullptr;
 AActor* Closest=nullptr;
 double Distance=FMath::Square(345.);
 for(TActorIterator<AStaticMeshActor> It(GetWorld());It;++It)
 {
  if(!It->ActorHasTag(TEXT("DungeonSecret")) || It->IsHidden() || It->Tags.Num()<2) continue;
  const FVector Offset=It->GetActorLocation()-GetActorLocation();
  const double Candidate=Offset.SizeSquared();
  if(Candidate<Distance && CombatVisible(this,*It))
  {
   Closest=*It;
   Distance=Candidate;
  }
 }
 return Closest;
}

void ACryptHero::NoticeEnemy(ACryptMonster* Enemy)
{
 CombatEnemy=Enemy;
 CombatEnemyTime=GetWorld()->GetTimeSeconds();
}

const ACryptMonster* ACryptHero::GetCombatEnemy() const
{
 const auto* Enemy=CombatEnemy.Get();
 return Enemy && Enemy->GetRemainingHealth()>0.f && GetWorld()->GetTimeSeconds()-CombatEnemyTime<8.f
  ? Enemy : nullptr;
}

void ACryptHero::ReportEnemyHit(ACryptMonster* Enemy,float Damage)
{
 NoticeEnemy(Enemy);
 if(Enemy->GetRemainingHealth()>0.f)
 {
  AppendEvent(FString::Printf(TEXT("Hit %s for %d."),Enemy->Stats().Name,FMath::CeilToInt(Damage)));
  return;
 }
 AddExperience(Enemy->IsBoss()?150:35);
 const int32 Item=FMath::RandRange(0,RelicCount-1);
 Relics.Add(Item);
 AppendEvent(FString::Printf(TEXT("%s defeated. Looted %s."),Enemy->Stats().Name,*RelicName(Item)));
 const int32 Shape=Item%RelicShapes;
 const FString Path=FString::Printf(TEXT("/Game/Characters/Town/DungeonAssets/Loot%02d.Loot%02d"),Shape,Shape);
 auto* RelicMesh=LoadObject<UStaticMesh>(nullptr,*Path);
 if(!RelicMesh) return;
 const FVector At=Enemy->GetActorLocation()+FVector(0,0,8);
 if(auto* Display=GetWorld()->SpawnActor<AStaticMeshActor>(At,FRotator(0,Item%360,0)))
 {
  auto* Visual=Display->GetStaticMeshComponent();
  Visual->SetMobility(EComponentMobility::Movable);
  Visual->SetStaticMesh(RelicMesh);
  Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  const int32 Finish=(Item/RelicShapes)%RelicFinishes;
  auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
  if(Base)
  {
   auto* Tint=UMaterialInstanceDynamic::Create(Base,Display);
   Tint->SetVectorParameterValue(TEXT("Color"),FLinearColor::MakeFromHSV8(uint8(Finish*16),170,215));
   Visual->SetMaterial(0,Tint);
  }
  Display->SetActorScale3D(FVector(1.f+.025f*(Item/(RelicShapes*RelicFinishes))));
  Display->SetLifeSpan(3.f);
 }
}

FString ACryptHero::GetRelicSummary() const
{
 return Relics.IsEmpty()?FString(TEXT("No relics collected")):
  FString::Printf(TEXT("Relics %d  |  %s"),Relics.Num(),*RelicName(Relics.Last()));
}

int32 ACryptHero::FindMerchant() const
{
 if(ChartIndex!=INDEX_NONE) return INDEX_NONE;
 double Nearest=FMath::Square(290.);
 int32 Shop=INDEX_NONE;
 for(TActorIterator<AStaticMeshActor> It(GetWorld());It;++It)
 {
  if(!It->ActorHasTag(TEXT("TownMerchant")) || It->Tags.Num()<2) continue;
  const double Distance=FVector::DistSquared(GetActorLocation(),It->GetActorLocation());
  if(Distance<Nearest)
  {
   Nearest=Distance;
   Shop=FCString::Atoi(*It->Tags[1].ToString());
  }
 }
 return Shop;
}

int32 ACryptHero::StockItem() const
{
 if(MerchantId==INDEX_NONE) return INDEX_NONE;
 const int32 Pitch[]={0,2,4,7,8,9,10,11};
 const int32 Shape=Pitch[StockIndex];
 return Shape+((MerchantId+StockIndex*3)%16)*16+((MerchantId/6+StockIndex/4)%4)*256;
}

FString ACryptHero::GetGearSelection() const
{
 if(!Relics.IsValidIndex(GearIndex)) return TEXT("No loot in the bag");
 const int32 Item=Relics[GearIndex];
 const int32 Shape=Item%16;
 FString Info;
 if(Shape<4) Info=FString::Printf(TEXT("Weapon bonus %d"),FMath::RoundToInt(GearGrade(Item)*1.4f));
 else if(Shape<8) Info=FString::Printf(TEXT("Armor bonus %d"),FMath::RoundToInt(GearGrade(Item)*1.25f));
 else if(Shape<12) Info=FString::Printf(TEXT("Restore health and mana"));
 else Info=TEXT("Collectible");
 if(Item==LootWeapon || Item==LootArmor) Info+=TEXT(" (equipped)");
 return FString::Printf(TEXT("%d / %d   %s\n%s"),GearIndex+1,Relics.Num(),*RelicName(Item),*Info);
}

FString ACryptHero::GetMerchantStock() const
{
 const int32 Item=StockItem();
 return Item==INDEX_NONE ? FString() : FString::Printf(TEXT("%s   |   %d treasure"),*RelicName(Item),3+GearGrade(Item)*2);
}

FString ACryptHero::GetEquippedGear() const
{
 const FString Weapon=LootWeapon==INDEX_NONE?FString(TEXT("Base weapon")):RelicName(LootWeapon);
 const FString Armor=LootArmor==INDEX_NONE?FString(TEXT("Base armor")):RelicName(LootArmor);
 return FString::Printf(TEXT("%s  /  %s"),*Weapon,*Armor);
}

FString ACryptHero::GetGearAction() const
{
 if(!Relics.IsValidIndex(GearIndex)) return TEXT("Use");
 const int32 Shape=Relics[GearIndex]%16;
 if(Shape<8) return (Relics[GearIndex]==LootWeapon || Relics[GearIndex]==LootArmor)?TEXT("Unequip"):TEXT("Equip");
 if(Shape<12) return TEXT("Drink");
 return TEXT("Inspect");
}

void ACryptHero::UseGear()
{
 if(!Relics.IsValidIndex(GearIndex)) return;
 const int32 Item=Relics[GearIndex];
 const int32 Shape=Item%16;
 const FString Label=RelicName(Item);
 if(Shape<4)
 {
  if(StrikeDuration>0.f) return;
  if(LootWeapon==Item)
  {
   EquipWeapon(ECryptWeapon::Unarmed);
   InspectedItem=TEXT("Weapon unequipped.");
   return;
  }
  const ECryptWeapon Family=Shape==3?ECryptWeapon::Spear:ECryptWeapon::Sword;
  ActiveWeaponMask|=1<<int32(Family);
  EquipWeapon(Family);
  LootWeapon=Item;
  InspectedItem=FString::Printf(TEXT("Equipped %s."),*Label);
 }
 else if(Shape<8)
 {
  if(LootArmor==Item)
  {
   LootArmor=INDEX_NONE;
   InspectedItem=TEXT("Armor unequipped.");
   return;
  }
  LootArmor=Item;
  InspectedItem=FString::Printf(TEXT("Equipped %s."),*Label);
 }
 else if(Shape<12)
 {
  if(Health>=100.f && Mana>=75.f)
  {
   InspectedItem=TEXT("Health and mana are full.");
   return;
  }
  Health=FMath::Min(100.f,Health+18.f+GearGrade(Item)*4.f);
  Mana=FMath::Min(75.f,Mana+6.f+GearGrade(Item)*2.f);
  Relics.RemoveAt(GearIndex);
  GearIndex=FMath::Clamp(GearIndex,0,FMath::Max(0,Relics.Num()-1));
  InspectedItem=FString::Printf(TEXT("Drank %s."),*Label);
 }
 else
 {
  InspectedItem=Label;
  return;
 }
 AppendEvent(InspectedItem);
}

void ACryptHero::BuyGear()
{
 const int32 Item=StockItem();
 if(Item==INDEX_NONE || FindMerchant()!=MerchantId) return;
 const int32 Price=3+GearGrade(Item)*2;
 if(TreasureCount<Price)
 {
  InspectedItem=TEXT("Not enough treasure.");
  return;
 }
 TreasureCount-=Price;
 Relics.Add(Item);
 GearIndex=Relics.Num()-1;
 InspectedItem=FString::Printf(TEXT("Bought %s."),*RelicName(Item));
 AppendEvent(InspectedItem);
}

void ACryptHero::SellGear()
{
 if(MerchantId==INDEX_NONE || FindMerchant()!=MerchantId || !Relics.IsValidIndex(GearIndex)) return;
 const int32 Item=Relics[GearIndex];
 if(Item==LootWeapon || Item==LootArmor)
 {
  InspectedItem=TEXT("Unequip this item before selling.");
  return;
 }
 const int32 Price=1+GearGrade(Item);
 TreasureCount+=Price;
 Relics.RemoveAt(GearIndex);
 GearIndex=FMath::Clamp(GearIndex,0,FMath::Max(0,Relics.Num()-1));
 InspectedItem=FString::Printf(TEXT("Sold %s for %d."),*RelicName(Item),Price);
 AppendEvent(InspectedItem);
}

void ACryptHero::AddExperience(int32 Points)
{
 if(Level>=100) return;
 Experience+=Points;
 while(Level<100 && Experience>=ExperienceNeeded())
 {
  Experience-=ExperienceNeeded();
  ++Level;
  AppendEvent(FString::Printf(TEXT("Reached level %d."),Level));
 }
 if(Level==100) Experience=0;
}

FString ACryptHero::GetArrowLabel() const
{
 const TCHAR* Names[]={TEXT("Standard"),TEXT("Poison"),TEXT("Fire")};
 return FString::Printf(TEXT("%s: %d"),Names[ArrowType],Arrows[ArrowType]);
}

void ACryptHero::BuyArrows()
{
 constexpr int32 Bundle=12;
 const int32 Price=ArrowType==0?4:9;
 if(MerchantId==INDEX_NONE || FindMerchant()!=MerchantId) return;
 if(TreasureCount<Price || Arrows[ArrowType]>999-Bundle)
 {
  InspectedItem=TreasureCount<Price?TEXT("Not enough treasure."):TEXT("Quiver is full.");
  return;
 }
 TreasureCount-=Price;
 Arrows[ArrowType]+=Bundle;
 InspectedItem=FString::Printf(TEXT("Bought %d arrows. %s"),Bundle,*GetArrowLabel());
 AppendEvent(InspectedItem);
}

void ACryptHero::UpdateGearVisuals()
{
 const auto Update=[&](UStaticMeshComponent*& Part,const TCHAR* Name,bool bShown,FVector At,FRotator Rotation)
 {
  if(!bShown)
  {
   if(Part) Part->SetVisibility(false);
   return;
  }
  if(!Part)
  {
   const FString Path=FString::Printf(TEXT("/Game/Characters/Town/DungeonAssets/%s.%s"),Name,Name);
   auto* Asset=LoadObject<UStaticMesh>(nullptr,*Path);
   if(!Asset) return;
   Part=NewObject<UStaticMeshComponent>(this);
   AddInstanceComponent(Part);
   Part->SetupAttachment(GetRootComponent());
   Part->SetMobility(EComponentMobility::Movable);
   Part->SetStaticMesh(Asset);
   Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
   Part->RegisterComponent();
  }
  Part->SetVisibility(true);
  Part->SetWorldLocationAndRotation(At,Rotation);
 };
 const FVector Forward=GetActorForwardVector();
 const FRotator Facing(0,GetActorRotation().Yaw-90,0);
 const FVector Chest=GetMesh()->DoesSocketExist(TEXT("chest"))?GetMesh()->GetSocketLocation(TEXT("chest")):
  GetActorLocation()+FVector(0,0,38);
 const FVector Grip=GetMesh()->DoesSocketExist(TEXT("hand_L"))?GetMesh()->GetSocketLocation(TEXT("hand_L")):
  GetActorLocation()+Forward*36-GetActorRightVector()*28;
 const bool bBow=EquippedWeapon==ECryptWeapon::Bow;
 Update(BowVisual,TEXT("HeroBow"),bBow,Grip,FRotator(-20,Facing.Yaw,0));
 Update(ArmorVisual,TEXT("HeroArmor"),LootArmor!=INDEX_NONE,Chest+Forward*8,Facing);
 Update(QuiverVisual,TEXT("HeroQuiver"),bBow,Chest-Forward*21+GetActorRightVector()*13,FRotator(0,Facing.Yaw,23));
}

void ACryptHero::FireBow()
{
 if(Arrows[ArrowType]<=0)
 {
  AppendEvent(TEXT("No arrows of this type."));
  AttackCooldown=.5f;
  return;
 }
 if(Stamina<8.f) return;
 constexpr double Range=2150.;
 const FVector Start=GetActorLocation()+FVector(0,0,26);
 const FVector Aim=GetActorForwardVector();
 ACryptMonster* Target=nullptr;
 double Nearest=FMath::Square(Range);
 for(TActorIterator<ACryptMonster> It(GetWorld());It;++It)
 {
  if(It->GetRemainingHealth()<=0.f) continue;
  const FVector Offset=It->GetActorLocation()-Start;
  const double Distance=Offset.SizeSquared();
  if(Distance<Nearest && FVector::DotProduct(Aim,Offset.GetSafeNormal())>.89 && CombatVisible(this,*It))
  {
   Target=*It;
   Nearest=Distance;
  }
 }
 const FVector Desired=Target?Target->GetActorLocation():Start+Aim*Range;
 FCollisionQueryParams Query;
 Query.AddIgnoredActor(this);
 FHitResult Hit;
 const bool bHit=GetWorld()->LineTraceSingleByChannel(Hit,Start,Desired,ECC_Visibility,Query);
 const FVector Impact=bHit?Hit.ImpactPoint:Desired;
 const int32 FiredType=ArrowType;
 --Arrows[FiredType];
 Stamina-=8.f;
 AttackCooldown=.62f;
 PlayOceanAction(OceanAttack,.50f);

 if(auto* Streak=GetWorld()->SpawnActor<AStaticMeshActor>((Start+Impact)*.5,(Impact-Start).Rotation()))
 {
  auto* ArrowTrail=Streak->GetStaticMeshComponent();
  ArrowTrail->SetMobility(EComponentMobility::Movable);
  ArrowTrail->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
  ArrowTrail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Streak->SetActorScale3D(FVector(FMath::Max(.02,FVector::Dist(Start,Impact)/100.),.018,.018));
  Streak->SetLifeSpan(.095f);
 }
 ACryptMonster* Victim=bHit?Cast<ACryptMonster>(Hit.GetActor()):Target;
 if(Victim && Victim->TakeHit(WeaponDamage(),GetDexterity(),ECryptDamage::Physical) && FiredType!=0)
 {
  Victim->ApplyArrowEffect(FiredType==2);
  if(FiredType==2) ShowSpell(Impact-FVector(0,0,18),Impact+FVector(0,0,42),
      ECryptDamage::Fire,FLinearColor(1.f,.24f,.035f));
  AppendEvent(FiredType==2?TEXT("Fire arrow ignited the enemy."):TEXT("Poison arrow struck the enemy."));
 }
 if(FiredType==2 && bHit && Hit.GetComponent() && Hit.GetComponent()->ComponentHasTag(TEXT("FlammableBuilding")))
  IgniteBuilding(Impact+Hit.ImpactNormal*8.);
}

void ACryptHero::IgniteBuilding(FVector Position)
{
 auto* Egg=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
 auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
 for(int32 Layer=0;Layer<5;++Layer)
 {
  const FVector Point=Position+FVector((Layer%2?1:-1)*8,Layer*7-14,Layer*15);
  auto* Flame=GetWorld()->SpawnActor<AStaticMeshActor>(Point,FRotator(0,Layer*36,Layer*5));
  if(!Flame) continue;
  auto* Visual=Flame->GetStaticMeshComponent();
  Visual->SetMobility(EComponentMobility::Movable);
  Visual->SetStaticMesh(Egg);
  Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Visual->SetCastShadow(false);
  Flame->SetActorScale3D(FVector(.19,.22,.42));
  if(Base)
  {
   auto* Glow=UMaterialInstanceDynamic::Create(Base,Flame);
   Glow->SetVectorParameterValue(TEXT("Color"),FLinearColor(1.f,.20f+.09f*Layer,.028f));
   Visual->SetMaterial(0,Glow);
  }
  Flame->SetLifeSpan(7.f+Layer*.25f);
 }
 AppendEvent(TEXT("Fire arrow set the building alight."));
}
