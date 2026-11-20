#include "CryptHero.h"
#include "CryptMonster.h"
#include "EngineUtils.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMesh.h"
#include "Engine/GameViewportClient.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimSequence.h"
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
}
void ACryptHero::BeginPlay()
{
 Super::BeginPlay();
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
}
void ACryptHero::MoveForward(float V) { if(FMath::Abs(V)>.01f) AddMovementInput(FRotationMatrix(FRotator(0,GetControlRotation().Yaw,0)).GetUnitAxis(EAxis::X),V); }
void ACryptHero::MoveRight(float V) { if(FMath::Abs(V)>.01f) AddMovementInput(FRotationMatrix(FRotator(0,GetControlRotation().Yaw,0)).GetUnitAxis(EAxis::Y),V); }
void ACryptHero::Turn(float V) { AddControllerYawInput(V); }
void ACryptHero::LookUp(float V) { AddControllerPitchInput(V); }
void ACryptHero::Attack()
{
 if(AttackCooldown>0) return;
 AttackCooldown=.45f;
 PlayOceanAction(OceanAttack,.45f);
 for(TActorIterator<ACryptMonster> It(GetWorld());It;++It)
 {
  const FVector Offset=It->GetActorLocation()-GetActorLocation();
  if(Offset.Size()<260.f && FVector::DotProduct(GetActorForwardVector(),Offset.GetSafeNormal())>.25f)
   It->TakeHit(bSwordEquipped?2.f:1.f);
 }
}
void ACryptHero::SpinAttack()
{
 if(AttackCooldown>0) return;
 AttackCooldown=1.2f;
 PlayOceanAction(OceanSpin,1.2f);
 for(TActorIterator<ACryptMonster> It(GetWorld());It;++It)
  if(FVector::Dist(It->GetActorLocation(),GetActorLocation())<300.f) It->TakeHit(bSwordEquipped?2.f:1.f);
}
void ACryptHero::Interact() { /* Trace for treasure and dungeon portal actors to be implemented */ }
void ACryptHero::EquipSword(bool bEquip) { bSwordEquipped=bEquip; SwordVisual->SetVisibility(bEquip); }

void ACryptHero::TouchPressed(ETouchIndex::Type FingerIndex,FVector Location)
{
 if(!GEngine || !GEngine->GameViewport) return;
 FVector2D ViewSize;
 GEngine->GameViewport->GetViewportSize(ViewSize);
 if(ViewSize.X<=0 || ViewSize.Y<=0) return;
 const float S=FMath::Max(.7f,FMath::Min(ViewSize.X/1600.f,ViewSize.Y/900.f));
 auto Hit=[&](float X,float Y) { return FMath::Abs(Location.X-X)<78.f*S && FMath::Abs(Location.Y-Y)<78.f*S; };
 const float W=ViewSize.X,H=ViewSize.Y;
 // Right virtual thumb pad: a tap in its inner circle triggers jump.
 // Keep the outer ring available for normal right-stick camera movement.
 const FVector2D RightPadCenter(W*.875f,H*.855f);
 const FVector2D TouchPoint(Location.X,Location.Y);
 const float RightPadInnerRadius=66.f*S;
 if(FVector2D::Distance(TouchPoint,RightPadCenter)<=RightPadInnerRadius)
 {
  Jump();
  UE_LOG(LogTemp,Log,TEXT("MOBILE_RIGHT_PAD: center pressed, jump requested"));
  return;
 }
 if(Hit(W-110*S,105*S)) Jump();
 else if(Hit(W-280*S,105*S)) Attack();
 else if(Hit(W-110*S,275*S)) SpinAttack();
 else if(Hit(W-280*S,275*S)) ToggleInventory();
}
