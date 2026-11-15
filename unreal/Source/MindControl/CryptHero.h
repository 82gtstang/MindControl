#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CryptHero.generated.h"
UCLASS()
class MINDCONTROL_API ACryptHero : public ACharacter
{
 GENERATED_BODY()
public:
 ACryptHero();
 virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
 virtual void Tick(float DeltaSeconds) override;
 UFUNCTION(BlueprintCallable) void Attack();
 UFUNCTION(BlueprintCallable) void SpinAttack();
 UFUNCTION(BlueprintCallable) void Interact();
 UFUNCTION(BlueprintCallable) void EquipSword(bool bEquip);
 UPROPERTY(BlueprintReadOnly) bool bSwordEquipped = false;
 UPROPERTY(BlueprintReadOnly) int32 TreasureCount = 0;
protected:
 virtual void BeginPlay() override;
 void MoveForward(float Value);
 void MoveRight(float Value);
 void Turn(float Value);
 void LookUp(float Value);
 UPROPERTY() class USpringArmComponent* CameraBoom;
 UPROPERTY() class UCameraComponent* FollowCamera;
 UPROPERTY() class UStaticMeshComponent* SwordVisual;
 float AttackCooldown = 0;
};
