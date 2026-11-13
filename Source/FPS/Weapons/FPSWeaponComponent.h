#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "Netcode/FPSLagCompensationComponent.h"
#include "FPSWeaponComponent.generated.h"

class USkeletalMeshComponent;
class UAnimMontage;
class USoundBase;

/** Client-side hit stats. "Agreed" = server hit the same target the client saw hit. */
USTRUCT(BlueprintType)
struct FFPSShotStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 ShotsFired = 0;
	UPROPERTY(BlueprintReadOnly) int32 ClientHits = 0;
	UPROPERTY(BlueprintReadOnly) int32 ServerAgreed = 0;
	/** Server hit something the client didn't see hit. */
	UPROPERTY(BlueprintReadOnly) int32 PhantomHits = 0;

	float GetAgreementPercent() const { return ClientHits > 0 ? 100.f * ServerAgreed / ClientHits : 0.f; }
};

/**
 * Hitscan rifle.
 * Owning client: aims, spread + recoil, FX, sends shot + "time I saw it" to the server.
 * Server: checks the shot, REWINDS hitboxes to that time, traces, restores, applies damage.
 */
UCLASS(ClassGroup = (FPS), meta = (BlueprintSpawnableComponent))
class FPS_API UFPSWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFPSWeaponComponent();

	void SetupMeshes(USkeletalMeshComponent* InArmsMesh, USkeletalMeshComponent* InWeaponFP, USkeletalMeshComponent* InWeaponTP);

	/** Owning client only. Called by the fire ability. */
	void StartFiring();
	void StopFiring();

	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetCurrentSpread() const;

	// ---- Test / metrics (owning client) ----
	/** Auto-aim at Target's body hitbox, no spread/recoil. nullptr = off. */
	void SetAutoTestTarget(AActor* Target);
	bool IsAutoTesting() const { return AutoTestTarget.IsValid(); }
	const FFPSShotStats& GetStats() const { return Stats; }
	void ResetStats();
	bool WasLastShotRewound() const { return bLastShotRewound; }
	float GetLastRewindMs() const { return LastRewindMs; }

	/** Overheat for the HUD (owning client's predicted value). 0-1. */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetHeatPercent() const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsOverheated() const;

	/** For the HUD hit marker (client, server-confirmed hits only). */
	float GetLastHitTime() const { return LastHitTime; }
	bool WasLastHitHeadshot() const { return bLastHitHeadshot; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	// ---- Damage ----
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Damage")
	float BodyDamage = 20.f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Damage")
	float HeadshotMultiplier = 2.f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Damage", meta = (Units = "cm"))
	float Range = 10000.f;

	/** Co-op: off. Console "fps.FriendlyFire 1" turns it on for tests. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Damage")
	bool bAllowFriendlyFire = false;

	// ---- Fire rate ----
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Fire", meta = (ClampMin = 1, Units = "Hz"))
	float ShotsPerSecond = 10.f;

	// ---- Overheat (no reload anim needed: hold the trigger too long and the gun locks) ----
	/** Heat added per shot (0.02 = 50 shots of non-stop fire). 0 = overheat off. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Heat", meta = (ClampMin = 0, ClampMax = 1))
	float HeatPerShot = 0.02f;

	/** Heat only starts dropping after this long without a shot. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Heat", meta = (Units = "s"))
	float HeatCoolDelay = 0.3f;

	/** Heat lost per second while cooling (0.4 = full bar in 2.5 s). */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Heat")
	float HeatCoolRate = 0.4f;

	/** Can't fire this long after overheating. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Heat", meta = (Units = "s"))
	float OverheatLockout = 1.5f;

	// ---- Spread (cone half-angle, degrees) ----
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Spread")
	float BaseSpread = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Spread")
	float SpreadPerShot = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Spread")
	float MaxBloom = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Spread")
	float BloomRecoveryRate = 6.f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Spread")
	float MovingSpread = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Spread")
	float AirSpread = 3.f;

	// ---- Recoil (degrees per shot) ----
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Recoil")
	float RecoilPitch = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Recoil")
	float RecoilYawRange = 0.2f;

	// ---- FX ----
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|FX")
	TObjectPtr<UAnimMontage> FireMontageFP;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|FX")
	TObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|FX")
	TObjectPtr<USoundBase> HitConfirmSound;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|FX")
	FName MuzzleSocketName = TEXT("Muzzle");

private:
	void FireShot();
	bool CanFire() const;
	FVector GetMuzzleLocation(bool bFirstPerson) const;
	bool GetAutoTestAimPoint(FVector& OutPoint) const;

	/** TargetTime = server time of the world the shooter saw when firing. */
	UFUNCTION(Server, Reliable)
	void ServerFire(FVector_NetQuantize Start, FVector_NetQuantizeNormal Direction, double TargetTime, int32 ShotId);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShotFX(FVector_NetQuantize TraceEnd);

	/** Result of one shot. DebugBoxes = hitboxes where the server had them (rewound), only when fps.NetDebug is on. */
	UFUNCTION(Client, Reliable)
	void ClientShotResult(int32 ShotId, AActor* HitActor, bool bHeadshot, bool bKill, float Damage,
		bool bRewound, float RewindMs, const TArray<FFPSDebugHitbox>& DebugBoxes);

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> ArmsMesh;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> WeaponMeshFP;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> WeaponMeshTP;

	/** Heat is computed from timestamps on demand: no tick needed. */
	struct FHeatState
	{
		float Heat = 0.f;              // value right after the last shot
		float LastShotTime = -1000.f;
		float OverheatUntil = -1000.f;
	};
	float GetHeatAt(const FHeatState& State, float Now) const;
	/** Adds one shot of heat. Returns true if this shot overheated the gun. */
	bool AddHeat(FHeatState& State, float Now, float OverheatAt, float Lockout) const;
	bool IsHeatEnabled() const;

	FHeatState ClientHeat;  // owning client: prediction, drives the HUD
	FHeatState ServerHeat;  // server: authority, rejects shots while overheated

	FTimerHandle FireTimer;
	float LastFireTime = -1000.f;          // client
	float ServerLastFireTime = -1000.f;    // server
	float CurrentBloom = 0.f;

	// Client: shot id -> actor our local trace hit (null = we saw a miss).
	int32 NextShotId = 0;
	TMap<int32, TWeakObjectPtr<AActor>> PendingShots;
	FFPSShotStats Stats;
	bool bLastShotRewound = false;
	float LastRewindMs = 0.f;
	float LastHitTime = -1000.f;
	bool bLastHitHeadshot = false;

	TWeakObjectPtr<AActor> AutoTestTarget;
};
