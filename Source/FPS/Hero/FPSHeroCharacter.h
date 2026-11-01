#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "FPSHeroCharacter.generated.h"

class UAbilitySystemComponent;
class UFPSAttributeSet;
class UFPSGameplayAbility;
class UFPSInputConfig;
class UInputAction;
class UInputMappingContext;
class UCameraComponent;
struct FInputActionValue;

/**
 * Multiplayer hero. ASC lives on AFPSPlayerState (Mixed replication).
 * Init: server in PossessedBy, client in OnRep_PlayerState.
 */
UCLASS(Abstract)
class FPS_API AFPSHeroCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AFPSHeroCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UFPSAttributeSet* GetAttributeSet() const;

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void PawnClientRestart() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Hero")
	bool IsDead() const { return bIsDead; }

	/** Server only. Called by the attribute set when health hits 0. */
	void HandleDeath();

	// ---- Debug console commands (type in ~ console) ----
	/** Damage every other hero. Usage: DebugDamageOthers 30 */
	UFUNCTION(Exec)
	void DebugDamageOthers(float Amount);

	/** Damage yourself. Usage: DebugDamageSelf 30 */
	UFUNCTION(Exec)
	void DebugDamageSelf(float Amount);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	/** Arms mesh, owner only. Set the template's FP arms here in the BP. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	// ---- Input ----
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UFPSInputConfig> InputConfig;

	// ---- GAS ----
	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TArray<TSubclassOf<UFPSGameplayAbility>> DefaultAbilities;

	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	float DefaultMaxHealth = 100.f;

	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	float DefaultMaxShield = 50.f;

	UPROPERTY(EditDefaultsOnly, Category = "Hero")
	float RespawnDelay = 3.f;

	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bShowDebugReadout = true;

	UPROPERTY(ReplicatedUsing = OnRep_IsDead, BlueprintReadOnly, Category = "Hero")
	bool bIsDead = false;

	UFUNCTION()
	void OnRep_IsDead();

private:
	void InitAbilitySystem();
	void ApplyDeathState();
	void Respawn();

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void AbilityInputPressed(FGameplayTag InputTag);
	void AbilityInputReleased(FGameplayTag InputTag);

	UFUNCTION(Server, Reliable)
	void ServerDebugDamage(bool bOthers, float Amount);

	void ApplyDamageTo(AFPSHeroCharacter* Target, float Amount);

	FTimerHandle RespawnTimer;
};
