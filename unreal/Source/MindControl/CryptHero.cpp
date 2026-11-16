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
ACryptHero::ACryptHero()
{
 PrimaryActorTick.bCanEverTick = true;
 GetCharacterMovement()->MaxWalkSpeed = 520;
 GetCharacterMovement()->JumpZVelocity = 650;
 GetCharacterMovement()->AirControl = .45f;
 CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
 CameraBoom->SetupAttachment(RootComponent);
 CameraBoom->TargetArmLength = 380;
 CameraBoom->bUsePawnControlRotation = true;
 FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
 FollowCamera->SetupAttachment(CameraBoom);
 // Visible placeholder hero, until licensed skeletal character art is imported.
 UStaticMeshComponent* Torso=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeroTorso"));
 Torso->SetupAttachment(RootComponent);
 Torso->SetRelativeLocation(FVector(0,0,-12));
 Torso->SetRelativeScale3D(FVector(.65f,.42f,1.35f));
 UStaticMeshComponent* Head=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeroHead"));
 Head->SetupAttachment(RootComponent);
 Head->SetRelativeLocation(FVector(0,0,95));
 Head->SetRelativeScale3D(FVector(.42f,.42f,.42f));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
 if(Sphere.Succeeded()) { Torso->SetStaticMesh(Sphere.Object); Head->SetStaticMesh(Sphere.Object); }
 SwordVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SwordVisual"));
 SwordVisual->SetupAttachment(GetMesh(), TEXT("hand_rSocket"));
 SwordVisual->SetRelativeScale3D(FVector(.08f,.08f,1.0f));
 SwordVisual->SetVisibility(false);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 if(Cube.Succeeded()) SwordVisual->SetStaticMesh(Cube.Object);
}
void ACryptHero::BeginPlay() { Super::BeginPlay(); SetActorLocation(FVector(-2900,0,150),false,nullptr,ETeleportType::TeleportPhysics); }
void ACryptHero::Tick(float DeltaSeconds) { Super::Tick(DeltaSeconds); AttackCooldown = FMath::Max(0.f,AttackCooldown-DeltaSeconds); }
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
 if(Hit(W-110*S,H-125*S)) Jump();
 else if(Hit(W-280*S,H-125*S)) Attack();
 else if(Hit(W-110*S,H-300*S)) SpinAttack();
 else if(Hit(W-280*S,H-300*S)) ToggleInventory();
}
