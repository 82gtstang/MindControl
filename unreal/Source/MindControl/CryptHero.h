#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CryptQuestData.h"
#include "CryptHero.generated.h"
struct FCryptQuestMarker
{
 FVector Location;
 int32 QuestIndex;
};

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
 void DefeatedRoadRaider();
 void EquipWeapon(ECryptWeapon Weapon);
 bool OwnsWeapon(ECryptWeapon Weapon) const;
 FString GetQuestJournal() const;
 FString GetInteractionLabel() const;
 void SaveProgress();
 void RestoreProgress();
 void GetQuestMapMarkers(TArray<FCryptQuestMarker>& Markers) const;
 const TArray<FVector>& GetStrikeTrail() const { return StrikeTrail; }
 float GetStrikeStrength() const;
 FString SaveStatus;
 bool bSatchelJournal=false;
 int32 GetPackedObjectiveCount(int32 Index) const;
 FString InspectedItem;
 FString Dialogue;
 ECryptWeapon EquippedWeapon = ECryptWeapon::Unarmed;
 UPROPERTY(BlueprintReadOnly) bool bSwordEquipped = false;
 UPROPERTY(BlueprintReadOnly) int32 TreasureCount = 0;
 UPROPERTY(BlueprintReadOnly) bool bInventoryOpen = false;
 UFUNCTION(BlueprintCallable) void ToggleInventory() { bInventoryOpen = !bInventoryOpen; bSatchelJournal=false; InspectedItem.Reset(); }
 UFUNCTION(BlueprintCallable) void AddTreasure(int32 Amount = 1) { TreasureCount += Amount; }
 void TouchPressed(ETouchIndex::Type FingerIndex, FVector Location);
protected:
 virtual void BeginPlay() override;
 void MoveForward(float Value);
 void MoveRight(float Value);
 void Turn(float Value);
 void LookUp(float Value);
 UPROPERTY() class USpringArmComponent* CameraBoom;
 UPROPERTY() class UCameraComponent* FollowCamera;
 UPROPERTY() class UStaticMeshComponent* SwordVisual;
 UPROPERTY() class UStaticMeshComponent* WeaponHead;
 UPROPERTY() class UStaticMeshComponent* WeaponGrip;
 class ACryptTownActor* GetInteractionTarget() const;
 int32 ActiveWeaponMask = 1;
 int32 QuestProgress[CryptQuests::Count] = {};
 uint8 QuestState[CryptQuests::Count] = {};
 float WeaponDamage() const;
 float WeaponReach() const;
 void CycleWeapon();
 void StartStrike(bool bSpin);
 void UpdateStrike(float DeltaSeconds);
 void ResolveStrike();
 FName GripBone;
 TArray<FVector> StrikeTrail;
 float StrikeAge=0.f;
 float StrikeDuration=0.f;
 float TrailSampleTime=0.f;
 bool bSpinStrike=false;
 bool bStrikeResolved=false;
 ECryptWeapon StrikeWeapon=ECryptWeapon::Unarmed;
 float AttackCooldown = 0;
 float DialogueTime = 0.f;
 UPROPERTY() class UAnimSequence* OceanIdle = nullptr;
 UPROPERTY() class UAnimSequence* OceanWalk = nullptr;
 UPROPERTY() class UAnimSequence* OceanRun = nullptr;
 UPROPERTY() class UAnimSequence* OceanJump = nullptr;
 UPROPERTY() class UAnimSequence* OceanAttack = nullptr;
 UPROPERTY() class UAnimSequence* OceanSpin = nullptr;
 UPROPERTY() class UAnimSequence* ActiveOceanAnimation = nullptr;
 float OceanActionTime = 0.f;
 void PlayOceanAction(class UAnimSequence* Sequence, float Duration);
};
