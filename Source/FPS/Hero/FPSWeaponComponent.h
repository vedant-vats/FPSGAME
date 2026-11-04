#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "FPSLagCompensationComponent.h"
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

	/** On for testing (only players exist). Turn off when enemies arrive (W5). */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Damage")
	bool bAllowFriendlyFire = true;

	// ---- Fire rate ----
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Fire", meta = (ClampMin = 1, Units = "Hz"))
	float ShotsPerSecond = 10.f;

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
