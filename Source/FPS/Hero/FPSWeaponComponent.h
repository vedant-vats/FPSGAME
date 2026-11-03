#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "FPSWeaponComponent.generated.h"

class USkeletalMeshComponent;
class UAnimMontage;
class USoundBase;

/**
 * Hitscan rifle.
 * Owning client: aims, applies spread + recoil, plays FX, sends the shot to the server.
 * Server: checks the shot, traces, applies damage through GAS.
 * Week 3 adds rewind inside ServerFire.
 */
UCLASS(ClassGroup = (FPS), meta = (BlueprintSpawnableComponent))
class FPS_API UFPSWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFPSWeaponComponent();

	/** Hero passes its meshes in after components exist. */
	void SetupMeshes(USkeletalMeshComponent* InArmsMesh, USkeletalMeshComponent* InWeaponFP, USkeletalMeshComponent* InWeaponTP);

	/** Owning client only. Called by the fire ability. */
	void StartFiring();
	void StopFiring();

	/** Current cone half-angle in degrees (base + bloom + move/air). */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetCurrentSpread() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	// ---- Damage ----
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Damage")
	float BodyDamage = 20.f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Damage")
	float HeadshotMultiplier = 2.f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Damage", meta = (Units = "cm"))
	float Range = 10000.f;

	/** On for Week 2 testing (only players exist). Turn off when enemies arrive (W5). */
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

	/** Degrees per second bloom shrinks back. */
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

	// ---- Debug ----
	/** Green = client shot, red = server shot. */
	UPROPERTY(EditAnywhere, Category = "Weapon|Debug")
	bool bDrawDebugShots = true;

private:
	void FireShot();
	bool CanFire() const;
	FVector GetMuzzleLocation(bool bFirstPerson) const;

	UFUNCTION(Server, Reliable)
	void ServerFire(FVector_NetQuantize Start, FVector_NetQuantizeNormal Direction, float ClientServerTime);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShotFX(FVector_NetQuantize TraceEnd);

	UFUNCTION(Client, Unreliable)
	void ClientHitConfirm(bool bHeadshot, bool bKill, float Damage);

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> ArmsMesh;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> WeaponMeshFP;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> WeaponMeshTP;

	FTimerHandle FireTimer;
	float LastFireTime = -1000.f;   // client
	float ServerLastFireTime = -1000.f;
	float CurrentBloom = 0.f;
};
