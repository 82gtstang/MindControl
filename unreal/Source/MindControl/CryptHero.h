#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CryptQuestData.h"
#include "CryptCombat.h"
#include "CryptHero.generated.h"
enum class ECryptThumbAction : uint8
{
 Jump, Attack, Spin, Fire, Storm, Count
};

inline constexpr const TCHAR* CryptThumbActionLabels[]={TEXT("Jump"),TEXT("Attack"),TEXT("Spin"),TEXT("Fire"),TEXT("Storm")};
static_assert(UE_ARRAY_COUNT(CryptThumbActionLabels)==int32(ECryptThumbAction::Count));

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
 virtual void CalcCamera(float DeltaTime,struct FMinimalViewInfo& OutResult) override;
 virtual void Tick(float DeltaSeconds) override;
 UFUNCTION(BlueprintCallable) void Attack();
 UFUNCTION(BlueprintCallable) void SpinAttack();
 UFUNCTION(BlueprintCallable) void Interact();
 UFUNCTION(BlueprintCallable) void EquipSword(bool bEquip);
 void DefeatedRoadRaider();
 void ReceiveCombatHit(float Rating,float Dexterity,ECryptDamage Element);
 void CheckDungeonClear(int32 Dungeon);
 void ShowSpell(FVector From,FVector To,ECryptDamage Element,FLinearColor Color);
 int32 GetLevel() const { return Level; }
 int32 GetExperience() const { return Experience; }
 int32 ExperienceNeeded() const { return 100+(Level-1)*40; }
 FString GetArrowLabel() const;
 bool IsBowAiming() const { return bBowAiming; }
 float GetHealth() const { return Health; }
 float GetStamina() const { return Stamina; }
 float GetMana() const { return Mana; }
 float GetArmorRating() const;
 float GetDexterity() const;
 float WeaponDamage() const;
 FString GetCombatTarget() const;
 int32 GetChartIndex() const { return ChartIndex; }
 const TSet<int32>& GetChartTiles() const { return ChartTiles; }
 const TArray<FString>& GetEventLog() const { return EventLog; }
 int32 GetLogOffset() const { return LogOffset; }
 FString GetRelicSummary() const;
 FString GetGearSelection() const;
 FString GetMerchantStock() const;
 FString GetEquippedGear() const;
 FString GetGearAction() const;
 bool bGearPage=false;
 int32 MerchantId=INDEX_NONE;
 const class ACryptMonster* GetCombatEnemy() const;
 void NoticeEnemy(class ACryptMonster* Enemy);
 void ReportEnemyHit(class ACryptMonster* Enemy,float Damage);
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
 UFUNCTION(BlueprintCallable) void ToggleInventory() { EndBowAim(false); bInventoryOpen = !bInventoryOpen; bSatchelJournal=false; bGearPage=false; MerchantId=INDEX_NONE; InspectedItem.Reset(); }
 UFUNCTION(BlueprintCallable) void AddTreasure(int32 Amount = 1) { TreasureCount += Amount; }
 void TouchPressed(ETouchIndex::Type FingerIndex, FVector Location);
 void TouchReleased(ETouchIndex::Type FingerIndex,FVector Location);
 void TouchMoved(ETouchIndex::Type FingerIndex,FVector Location);
 ECryptThumbAction GetThumbAction() const { return ThumbAction; }
protected:
 virtual void BeginPlay() override;
 void SetThumbAction(ECryptThumbAction Action);
 ECryptThumbAction ThumbAction=ECryptThumbAction::Jump;
 int32 ThumbFinger=INDEX_NONE;
 FVector2D ThumbStart=FVector2D::ZeroVector;
 float ThumbStartTime=0.f;
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
 int32 ActiveWeaponMask = 17;
 int32 QuestProgress[CryptQuests::Count] = {};
 uint8 QuestState[CryptQuests::Count] = {};
 void AppendEvent(const FString& Message);
 void UpdateChart();
 AActor* FindSecret() const;
 void ScrollLog(int32 Delta);
 TSet<int32> ChartTiles;
 TSet<int32> OpenSecrets;
 TArray<FString> EventLog;
 TArray<int32> Relics;
 int32 GearIndex=0, StockIndex=0;
 int32 Level=1, Experience=0;
 int32 ArrowType=0;
 int32 Arrows[3]={30,10,10};
 UPROPERTY() class UStaticMeshComponent* BowVisual=nullptr;
 UPROPERTY() class UStaticMeshComponent* ArmorVisual=nullptr;
 UPROPERTY() class UStaticMeshComponent* QuiverVisual=nullptr;
 void AddExperience(int32 Points);
 void UpdateGearVisuals();
 void FireBow(bool bPrecise=false,FVector ShotStart=FVector::ZeroVector,FVector ShotDirection=FVector::ForwardVector);
 void AttackPressed();
 void AttackReleased();
 FVector GetBowAimOrigin() const;
 void BeginBowAim(int32 Finger);
 void EndBowAim(bool bShoot);
 bool bBowAiming=false;
 bool bAimWasHidden=false;
 int32 BowAimFinger=INDEX_NONE;
 FRotator PreAimRotation=FRotator::ZeroRotator;
 struct FFlameVisual
 {
  FVector Start=FVector::ZeroVector;
  FVector End=FVector::ZeroVector;
  TArray<TWeakObjectPtr<class AStaticMeshActor>> Parts;
  double Radius=0.;
  float Age=0.f;
  float Duration=0.f;
  bool bRising=false;
 };
 TArray<FFlameVisual> Flames;
 void SpawnFlame(FVector Start,FVector End,double Radius,float Duration,bool bRising=false,float Delay=0.f,bool bArrow=false);
 void UpdateFlames(float DeltaSeconds);
 void IgniteBuilding(FVector Position);
 void BuyArrows();
 int32 LootWeapon=INDEX_NONE, LootArmor=INDEX_NONE;
 int32 FindMerchant() const;
 int32 StockItem() const;
 void UseGear();
 void BuyGear();
 void SellGear();
 int32 ChartIndex=INDEX_NONE;
 int32 LogOffset=0;
 TWeakObjectPtr<class ACryptMonster> CombatEnemy;
 float CombatEnemyTime=0.f;
 void CastSpell(ECryptDamage Element);
 void CastFire() { CastSpell(ECryptDamage::Fire); }
 void CastStorm() { CastSpell(ECryptDamage::Storm); }
 float Health=100.f, Stamina=100.f, Mana=75.f;
 float RecoveryDelay=0.f, HurtCooldown=0.f, SpellCooldown=0.f;
 uint32 ClearedDungeons=0;
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
 UPROPERTY() class UAnimSequence* OceanWalkDiagonal = nullptr;
 UPROPERTY() class UAnimSequence* OceanRunDiagonal = nullptr;
 UPROPERTY() class UAnimSequence* OceanJump = nullptr;
 UPROPERTY() class UAnimSequence* OceanAttack = nullptr;
 UPROPERTY() class UAnimSequence* OceanSpin = nullptr;
 UPROPERTY() class UAnimSequence* OceanSwim = nullptr;
 UPROPERTY() class UAnimSequence* ActiveOceanAnimation = nullptr;
 UPROPERTY() class USoundBase* FantasyMusic = nullptr;
 UPROPERTY() class USoundBase* FootstepSound = nullptr;
 UPROPERTY() class USoundBase* SwimSound = nullptr;
 UPROPERTY() class USoundBase* PunchSound = nullptr;
 UPROPERTY() class USoundBase* SwordSound = nullptr;
 UPROPERTY() class UAudioComponent* MusicPlayer = nullptr;
 float MusicVolume = .45f;
 float EffectsVolume = .65f;
 float StepAudioTime = 0.f;
 float OceanActionTime = 0.f;
 void PlayOceanAction(class UAnimSequence* Sequence, float Duration);
};

