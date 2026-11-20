#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "Core/FPSCombatantInterface.h"
#include "FPSEnemyCharacter.generated.h"

class UAbilitySystemComponent;
class UFPSAttributeSet;
class UFPSLagCompensationComponent;
class USphereComponent;
class UCapsuleComponent;
class UShapeComponent;
class UAnimMontage;
class USoundBase;
class AFPSEnemySpawner;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnEnemyDied, AFPSEnemyCharacter*);

/** Charge steps (server). Windup = standing still, telegraphing. Running = straight-line bull rush. */
enum class EFPSChargeState : uint8
{
	None,
	Windup,
	Running
};

/**
 * Pooled PvE enemy. Stats live on the Blueprint's class defaults (shared by every enemy of that type).
 *  - ASC on itself, MINIMAL replication: clients only get tags + attributes.
 *  - Hitboxes on the CAPSULE (not bones): the server never needs enemy animation for hits.
 *  - Never destroyed: Activate() on spawn, Deactivate() back to the pool (dormant, hidden, no tick).
 */
UCLASS(Abstract)
class FPS_API AFPSEnemyCharacter : public ACharacter, public IAbilitySystemInterface, public IFPSCombatantInterface
{
	GENERATED_BODY()

public:
	AFPSEnemyCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override; // only enabled during a charge rush
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// IFPSCombatantInterface
	virtual bool IsCombatantDead() const override { return bIsDead; }
	virtual void OnCombatantDied() override;

	// ---- Pool (server) ----
	/** FloorLocation = point on the navmesh. Tier: 0 normal, 1 elite, 2 mini-boss. */
	void Activate(const FVector& FloorLocation, const FRotator& Rotation, uint8 Tier = 0);
	void Deactivate();
	bool IsActive() const { return bActive; }

	/** Spawner listens to count kills for waves. */
	FOnEnemyDied OnEnemyDied;

	// ---- Combat (server, called by the AI controller) ----
	bool IsStunned() const;
	bool CanAttack() const;
	void StartAttack(AActor* Target);
	float GetAttackRange() const { return AttackRange; }

	// ---- Charge (server, called by the AI controller). Only if bCanCharge is ticked. ----
	bool CanStartCharge(const AActor* Target) const;
	void StartCharge(AActor* Target);
	void EndCharge();
	bool IsChargeWindup() const { return ChargeState == EFPSChargeState::Windup; }
	bool IsChargeRunning() const { return ChargeState == EFPSChargeState::Running; }

	// ---- HUD ----
	const FText& GetDisplayName() const { return DisplayName; }
	uint8 GetTier() const { return Tier; }
	const UFPSAttributeSet* GetAttributeSet() const { return AttributeSet; }

protected:
	/** Shown above the health bar ("Runner", "Tank"). */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Stats")
	FText DisplayName = NSLOCTEXT("FPS", "EnemyName", "Enemy");

	// ---- Stats (set per enemy type in its Blueprint) ----
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Stats")
	float MaxHealth = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Stats", meta = (Units = "cm/s"))
	float MoveSpeed = 500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Attack")
	float AttackDamage = 10.f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Attack", meta = (Units = "cm"))
	float AttackRange = 150.f;

	/** Delay between starting the swing and the hit landing (gives players time to dodge). */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Attack", meta = (Units = "s"))
	float AttackWindup = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Attack", meta = (Units = "s"))
	float AttackCooldown = 1.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Attack")
	TObjectPtr<UAnimMontage> AttackMontage;

	// ---- Charge (tank): stop, telegraph, then sprint at a player who is kiting it ----
	/** Tick this on the tank Blueprint. */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Charge")
	bool bCanCharge = false;

	/** Only charge if the target is at least this far (closer = just walk and swing). */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Charge", meta = (Units = "cm", EditCondition = "bCanCharge"))
	float ChargeMinRange = 600.f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Charge", meta = (Units = "cm", EditCondition = "bCanCharge"))
	float ChargeMaxRange = 1500.f;

	/** Stand still this long before running (the player's warning). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Charge", meta = (Units = "s", EditCondition = "bCanCharge"))
	float ChargeWindup = 0.4f;

	/** Rush speed. Faster than the player on purpose: you dodge it sideways, you don't outrun it. */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Charge", meta = (Units = "cm/s", EditCondition = "bCanCharge"))
	float ChargeSpeed = 1200.f;

	/** The rush locks the target's position at the end of the windup and runs this far past it. */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Charge", meta = (Units = "cm", EditCondition = "bCanCharge"))
	float ChargeOvershoot = 300.f;

	/** Safety cap on the rush length. */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Charge", meta = (Units = "s", EditCondition = "bCanCharge"))
	float ChargeDuration = 1.5f;

	/** Charge hit shoves the player this hard, away from the tank. */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Charge", meta = (Units = "cm/s", EditCondition = "bCanCharge"))
	float ChargeKnockback = 600.f;

	/** Small upward pop so the shove doesn't drag on the floor. */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Charge", meta = (Units = "cm/s", EditCondition = "bCanCharge"))
	float ChargeKnockUp = 250.f;

	/** Rushing into a wall or cover stuns the tank this long (reward for baiting it). */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Charge", meta = (Units = "s", EditCondition = "bCanCharge"))
	float WallStunDuration = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Charge", meta = (Units = "s", EditCondition = "bCanCharge"))
	float ChargeCooldown = 6.f;

	/** Runs on every machine when the windup starts. Hook the warning visuals/sound here in the BP. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|Charge")
	void OnChargeWindup();

	/** Every machine: the rush starts moving (whoosh/roar). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|Charge")
	void OnChargeRunStarted();

	/** Every machine: the rush hit a player (bHitWall false) or a wall/cover/enemy (true: wall stun starts). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|Charge")
	void OnChargeImpact(bool bHitWall);

	/** Every machine: the wall stun is over (remove the dizzy effect). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|Charge")
	void OnWallStunEnd();

	/** Animation speed during the rush, so the legs keep up with the sprint (1 = normal). */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Charge", meta = (ClampMin = 0.5, ClampMax = 4, EditCondition = "bCanCharge"))
	float ChargeAnimPlayRate = 1.8f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Feel")
	TObjectPtr<USoundBase> DeathSound;

	// ---- Elite / mini-boss (Marvel Zombies style variants) ----
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Elite")
	float EliteScale = 1.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Elite")
	float EliteHealthMultiplier = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Elite")
	float EliteDamageMultiplier = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Elite")
	float BossScale = 2.f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Elite")
	float BossHealthMultiplier = 12.f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Elite")
	float BossDamageMultiplier = 2.f;

	/** 0 normal, 1 elite, 2 mini-boss. Replicated so clients scale the body too. */
	UPROPERTY(ReplicatedUsing = OnRep_Tier)
	uint8 Tier = 0;

	UFUNCTION()
	void OnRep_Tier();

	/** Seconds the body stays after death before it returns to the pool. */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Stats", meta = (Units = "s"))
	float CorpseTime = 2.f;

	// ---- Components ----
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UFPSAttributeSet> AttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UFPSLagCompensationComponent> LagCompensationComponent;

	/** On the capsule at a fixed height. Move/resize in the BP viewport to fit the mesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hitboxes")
	TObjectPtr<USphereComponent> HeadHitbox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hitboxes")
	TObjectPtr<UCapsuleComponent> BodyHitbox;

	UPROPERTY(ReplicatedUsing = OnRep_IsDead)
	bool bIsDead = false;

	UFUNCTION()
	void OnRep_IsDead();

private:
	static void ConfigureHitbox(UShapeComponent* Hitbox);
	void SetHitboxesEnabled(bool bEnabled);
	void ApplyDeathVisuals();
	void ResetVisuals();
	void FinishAttack();
	void OnStunTagChanged(const FGameplayTag Tag, int32 NewCount);
	void ApplyTierScale();
	float GetTierHealthMultiplier() const;
	float GetTierDamageMultiplier() const;

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayAttack();

	void BeginChargeRun();
	void ChargeHitPlayer(AActor* Target);
	void ChargeHitWall();
	void ClearWallStun();

	/** Reliable: the warning is what makes the charge fair, so it must arrive. Rare (cooldown), so cheap. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastChargeWindup();

	// Cosmetic only, so Unreliable. The anim rate is also reset in ResetVisuals/ApplyDeathVisuals,
	// so a dropped "ended" message can't leave a tank running in fast-forward.
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastChargeRunStarted();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastChargeRunEnded();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastChargeImpact(bool bHitWall);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastWallStunEnd();

	EFPSChargeState ChargeState = EFPSChargeState::None;
	float NextChargeTime = 0.f;
	float ChargeElapsed = 0.f;
	FVector ChargeDirection = FVector::ForwardVector;
	FVector ChargeEndPoint = FVector::ZeroVector;
	TWeakObjectPtr<AActor> ChargeTarget;
	float DefaultMaxAcceleration = 0.f;
	bool bWallStunned = false;
	FTimerHandle ChargeTimer;
	FTimerHandle WallStunTimer;

	bool bActive = false;
	float NextAttackTime = 0.f;
	TWeakObjectPtr<AActor> AttackTarget;
	FTimerHandle AttackTimer;
	FTimerHandle CorpseTimer;
};
