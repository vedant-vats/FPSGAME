#include "FPSWeaponComponent.h"
#include "FPSCollision.h"
#include "FPSDamageEffect.h"
#include "FPSGameplayTags.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UFPSWeaponComponent::UFPSWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true); // needed for RPCs on a component
}

void UFPSWeaponComponent::SetupMeshes(USkeletalMeshComponent* InArmsMesh, USkeletalMeshComponent* InWeaponFP, USkeletalMeshComponent* InWeaponTP)
{
	ArmsMesh = InArmsMesh;
	WeaponMeshFP = InWeaponFP;
	WeaponMeshTP = InWeaponTP;
}

// ---------------------------------------------------------------- Client side

void UFPSWeaponComponent::StartFiring()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Interval = 1.f / ShotsPerSecond;
	const float Wait = FMath::Max(0.f, LastFireTime + Interval - World->GetTimeSeconds());

	if (Wait <= 0.f)
	{
		FireShot();
		World->GetTimerManager().SetTimer(FireTimer, this, &ThisClass::FireShot, Interval, true);
	}
	else
	{
		// Clicked too fast: wait for the refire time, no extra shots.
		World->GetTimerManager().SetTimer(FireTimer, this, &ThisClass::FireShot, Interval, true, Wait);
	}
}

void UFPSWeaponComponent::StopFiring()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FireTimer);
	}
}

bool UFPSWeaponComponent::CanFire() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return false;
	}

	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
	return !(ASC && ASC->HasMatchingGameplayTag(FPSTags::State_Dead));
}

float UFPSWeaponComponent::GetCurrentSpread() const
{
	float Spread = BaseSpread + CurrentBloom;

	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (Character->GetCharacterMovement()->IsFalling())
		{
			Spread += AirSpread;
		}
		else if (Character->GetVelocity().Size2D() > 50.f)
		{
			Spread += MovingSpread;
		}
	}
	return Spread;
}

void UFPSWeaponComponent::FireShot()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UWorld* World = GetWorld();
	if (!PC || !World || !CanFire())
	{
		StopFiring();
		return;
	}

	LastFireTime = World->GetTimeSeconds();

	// 1. Aim from the camera, add random spread inside a cone.
	FVector EyeLocation;
	FRotator EyeRotation;
	PC->GetPlayerViewPoint(EyeLocation, EyeRotation);

	const float HalfAngleRad = FMath::DegreesToRadians(GetCurrentSpread());
	const FVector ShotDirection = FMath::VRandCone(EyeRotation.Vector(), HalfAngleRad);

	// 2. Bloom grows each shot.
	CurrentBloom = FMath::Min(CurrentBloom + SpreadPerShot, MaxBloom);

	// 3. Recoil kicks the camera up and a bit sideways.
	PC->SetControlRotation(PC->GetControlRotation() + FRotator(RecoilPitch, FMath::FRandRange(-RecoilYawRange, RecoilYawRange), 0.f));

	// 4. Local trace: only for visuals. The server decides real hits.
	const FVector TraceEnd = EyeLocation + ShotDirection * Range;
	FHitResult LocalHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FPSWeaponLocalTrace), false, Pawn);
	const bool bLocalHit = World->LineTraceSingleByChannel(LocalHit, EyeLocation, TraceEnd, ECC_Weapon, Params);
	const FVector VisualEnd = bLocalHit ? LocalHit.ImpactPoint : TraceEnd;

	// 5. Local FX.
	if (ArmsMesh && FireMontageFP)
	{
		if (UAnimInstance* Anim = ArmsMesh->GetAnimInstance())
		{
			Anim->Montage_Play(FireMontageFP);
		}
	}
	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetMuzzleLocation(true));
	}
	if (bDrawDebugShots)
	{
		DrawDebugLine(World, GetMuzzleLocation(true), VisualEnd, FColor::Green, false, 2.f, 0, 0.5f);
	}

	// 6. Send to server. ClientServerTime = our guess of server time (used by rewind in Week 3).
	const AGameStateBase* GameState = World->GetGameState();
	const float ClientServerTime = GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
	ServerFire(EyeLocation, ShotDirection, ClientServerTime);
}

// ---------------------------------------------------------------- Server side

void UFPSWeaponComponent::ServerFire_Implementation(FVector_NetQuantize Start, FVector_NetQuantizeNormal Direction, float ClientServerTime)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	UWorld* World = GetWorld();
	if (!Pawn || !World || !CanFire())
	{
		return;
	}

	// Check 1: fire rate. Allow some slack for network jitter.
	const float Now = World->GetTimeSeconds();
	const float Interval = 1.f / ShotsPerSecond;
	if (Now - ServerLastFireTime < Interval * 0.5f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Weapon] %s fired too fast, shot rejected"), *Pawn->GetName());
		return;
	}
	ServerLastFireTime = Now;

	// Check 2: shot must start near the shooter.
	if (FVector::Dist(Start, Pawn->GetActorLocation()) > 200.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Weapon] %s shot start too far from pawn, rejected"), *Pawn->GetName());
		return;
	}

	// Server trace. NO rewind yet: at high ping, moving targets will be missed. That's the Week 3 "before" number.
	const FVector End = Start + Direction * Range;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FPSWeaponServerTrace), false, Pawn);
	const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Weapon, Params);
	const FVector HitEnd = bHit ? Hit.ImpactPoint : End;

	if (bDrawDebugShots)
	{
		DrawDebugLine(World, Start, HitEnd, FColor::Red, false, 2.f, 0, 0.5f);
		if (bHit)
		{
			DrawDebugPoint(World, Hit.ImpactPoint, 10.f, FColor::Red, false, 2.f);
		}
	}

	MulticastShotFX(HitEnd);

	if (!bHit)
	{
		return;
	}

	AActor* HitActor = Hit.GetActor();
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(HitActor);
	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
	if (!TargetASC || !SourceASC || TargetASC->HasMatchingGameplayTag(FPSTags::State_Dead))
	{
		return;
	}

	const APawn* TargetPawn = Cast<APawn>(HitActor);
	if (!bAllowFriendlyFire && TargetPawn && TargetPawn->IsPlayerControlled())
	{
		return;
	}

	const bool bHeadshot = Hit.GetComponent() && Hit.GetComponent()->ComponentHasTag(FPS_HEAD_HITBOX_TAG);
	const float Damage = BodyDamage * (bHeadshot ? HeadshotMultiplier : 1.f);

	UFPSDamageEffect::ApplyDamage(SourceASC, TargetASC, Damage, Pawn);

	const bool bKill = TargetASC->HasMatchingGameplayTag(FPSTags::State_Dead);
	const float ServerTimeDelta = (World->GetGameState() ? World->GetGameState()->GetServerWorldTimeSeconds() : Now) - ClientServerTime;

	UE_LOG(LogTemp, Log, TEXT("[Weapon] %s hit %s (%s) for %.0f%s | shot age %.0f ms"),
		*Pawn->GetName(), *GetNameSafe(HitActor), *GetNameSafe(Hit.GetComponent()), Damage,
		bKill ? TEXT(" KILL") : TEXT(""), ServerTimeDelta * 1000.f);

	ClientHitConfirm(bHeadshot, bKill, Damage);
}

// ---------------------------------------------------------------- FX for others

void UFPSWeaponComponent::MulticastShotFX_Implementation(FVector_NetQuantize TraceEnd)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || Pawn->IsLocallyControlled())
	{
		return; // shooter already played local FX
	}

	const FVector Muzzle = GetMuzzleLocation(false);
	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, Muzzle);
	}
	if (bDrawDebugShots)
	{
		DrawDebugLine(GetWorld(), Muzzle, TraceEnd, FColor::Yellow, false, 1.f, 0, 0.5f);
	}
}

void UFPSWeaponComponent::ClientHitConfirm_Implementation(bool bHeadshot, bool bKill, float Damage)
{
	if (HitConfirmSound)
	{
		UGameplayStatics::PlaySound2D(this, HitConfirmSound);
	}
	if (GEngine)
	{
		const FString Msg = FString::Printf(TEXT("%s %.0f%s"), bHeadshot ? TEXT("HEADSHOT") : TEXT("HIT"), Damage, bKill ? TEXT("  KILL") : TEXT(""));
		GEngine->AddOnScreenDebugMessage(2000, 0.5f, bHeadshot ? FColor::Orange : FColor::White, Msg);
	}
}

// ---------------------------------------------------------------- Helpers

FVector UFPSWeaponComponent::GetMuzzleLocation(bool bFirstPerson) const
{
	const USkeletalMeshComponent* Mesh = bFirstPerson ? WeaponMeshFP.Get() : WeaponMeshTP.Get();
	if (Mesh && Mesh->DoesSocketExist(MuzzleSocketName))
	{
		return Mesh->GetSocketLocation(MuzzleSocketName);
	}
	if (Mesh)
	{
		return Mesh->GetComponentLocation();
	}
	return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

void UFPSWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Bloom shrinks when not shooting.
	CurrentBloom = FMath::FInterpConstantTo(CurrentBloom, 0.f, DeltaTime, BloomRecoveryRate);
}
