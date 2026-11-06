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
class UFPSWeaponComponent;
class UFPSLagCompensationComponent;
class USoundBase;
class UNiagaraSystem;
class USphereComponent;
class UCapsuleComponent;
class UShapeComponent;
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

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void PawnClientRestart() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Hero")
	bool IsDead() const { return bIsDead; }

	/** For anim BPs (ABP_FP_Hero reads the camera to aim the arms). */
	UFUNCTION(BlueprintPure, Category = "Hero")
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	UFUNCTION(BlueprintPure, Category = "Hero")
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

	/** Server only. Called by the attribute set when health hits 0. */
	void HandleDeath();

	UFUNCTION(BlueprintPure, Category = "Hero")
	bool IsStunned() const;

	// ---- Ability feel ----
	/** Local: widen FOV by Degrees, eases back (dash punch). */
	void AddFovKick(float Degrees);

	/** Local: play sound + VFX on this hero. bAttach = follow the hero (trail), else at feet (ring). */
	void PlayAbilityFX(USoundBase* Sound, UNiagaraSystem* VFX, bool bAttach);

	/** Server -> everyone except the owner (owner already played it). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastAbilityFX(USoundBase* Sound, UNiagaraSystem* VFX, bool bAttach);

	virtual bool CanJumpInternal_Implementation() const override;

	// ---- Debug console commands (type in ~ console) ----
	/** Damage every other hero. Usage: DebugDamageOthers 30 */
	UFUNCTION(Exec)
	void DebugDamageOthers(float Amount);

	/** Damage yourself. Usage: DebugDamageSelf 30 */
	UFUNCTION(Exec)
	void DebugDamageSelf(float Amount);

	/** Netcode test: auto-aim at the nearest other hero and fire non-stop. Usage: DebugAutoTest 1 / 0 */
	UFUNCTION(Exec)
	void DebugAutoTest(int32 bEnable);

	/** Netcode test: strafe left/right on your own. Usage: DebugStrafe 1 / 0 */
	UFUNCTION(Exec)
	void DebugStrafe(int32 bEnable);

	/** Zero the hit stats shown on screen. */
	UFUNCTION(Exec)
	void DebugResetStats();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	/** Full-body FP mesh, owner only (template style). Camera sits on its head bone, so it MUST have a mesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> WeaponMeshFP;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> WeaponMeshTP;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UFPSWeaponComponent> WeaponComponent;

	/** Records hitbox history (server) and syncs the clock (owning client). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UFPSLagCompensationComponent> LagCompensationComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hitboxes")
	TObjectPtr<USphereComponent> HeadHitbox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hitboxes")
	TObjectPtr<UCapsuleComponent> BodyHitbox;

	// ---- Input ----
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** Extra contexts, e.g. IMC_Weapons (has IA_Shoot). */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TArray<TObjectPtr<UInputMappingContext>> AdditionalMappingContexts;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MouseLookAction;

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

	/** Bones hidden on the FP mesh so the camera doesn't see inside the head. */
	UPROPERTY(EditDefaultsOnly, Category = "Components")
	TArray<FName> FirstPersonHiddenBones = { TEXT("head") };

	UPROPERTY(ReplicatedUsing = OnRep_IsDead, BlueprintReadOnly, Category = "Hero")
	bool bIsDead = false;

	UFUNCTION()
	void OnRep_IsDead();

private:
	static void ConfigureHitbox(UShapeComponent* Hitbox);
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

	void DrawDebugReadout();
	void TickDebugStrafe(float DeltaSeconds);

	FTimerHandle RespawnTimer;

	float BaseFov = 90.f;
	float FovKickCurrent = 0.f;

	bool bDebugStrafe = false;
	float StrafeDirection = 1.f;
	float StrafeTimer = 0.f;
};
