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
 CameraBoom->TargetArmLength = 520;
 CameraBoom->bDoCollisionTest = true;
 CameraBoom->bUsePawnControlRotation = true;
 FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
 FollowCamera->SetupAttachment(CameraBoom);
 // Prefer the imported Ocean Serenity rig; retain Countess and silhouette fallbacks.
 static ConstructorHelpers::FObjectFinder<USkeletalMesh> OceanMesh(TEXT("/Game/Characters/OceanSerenity/OceanSerenity.OceanSerenity"));
 // Load the licensed Countess mesh if its Fab content has been installed locally.
 static ConstructorHelpers::FObjectFinder<USkeletalMesh> CountessMesh(TEXT("/Game/ParagonCountess/Characters/Heroes/Countess/Meshes/Countess.Countess"));
 const bool bHasOcean = OceanMesh.Succeeded();
 const bool bHasCountess = !bHasOcean && CountessMesh.Succeeded();
 if(bHasOcean)
 {
  GetMesh()->SetSkeletalMesh(OceanMesh.Object);
  // Blender rig is grounded at its feet, unlike the usual UE mannequin
  // whose mesh origin is at capsule center. Keep Ocean at capsule base
  // and allow a little clearance above the floor.
  GetMesh()->SetRelativeLocation(FVector(0,0,-75));
  // The Blender FBX arrives lying on its back in the Android build.
  // Rotate only the skeletal mesh about its local X axis to stand it upright;
  // do not rotate the capsule, movement directions, or follow camera.
  GetMesh()->SetRelativeRotation(FRotator(0,-90,-90));
  GetMesh()->SetRelativeScale3D(FVector(1,1,1));
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
 SetActorLocation(FVector(-2900,0,150),false,nullptr,ETeleportType::TeleportPhysics);
 // Align the lowest point of the imported mesh with the collision capsule base.
 // Imported FBX origins are not guaranteed to match Unreal mannequin origins.
 if (GetMesh() && GetMesh()->GetSkeletalMeshAsset())
 {
  GetMesh()->UpdateBounds();
  const FBoxSphereBounds MeshBounds = GetMesh()->Bounds;
  const float MeshBottom = MeshBounds.Origin.Z - MeshBounds.BoxExtent.Z;
  const float CapsuleBottom = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
  const float Adjustment = CapsuleBottom - MeshBottom + 2.0f;
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
 }
}
void ACryptHero::PlayOceanAction(UAnimSequence* Sequence,float Duration)
{
 if (!Sequence || !GetMesh()) return;
 ActiveOceanAnimation=Sequence;
 OceanActionTime=Duration;
 GetMesh()->PlayAnimation(Sequence,false);
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
 if(Hit(W-110*S,105*S)) Jump();
 else if(Hit(W-280*S,105*S)) Attack();
 else if(Hit(W-110*S,275*S)) SpinAttack();
 else if(Hit(W-280*S,275*S)) ToggleInventory();
}
