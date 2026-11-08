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

	bool bActive = false;
	float NextAttackTime = 0.f;
	TWeakObjectPtr<AActor> AttackTarget;
	FTimerHandle AttackTimer;
	FTimerHandle CorpseTimer;
};
