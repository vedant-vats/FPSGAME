#include "FPSWeaponComponent.h"
#include "FPSCollision.h"
#include "FPSDamageEffect.h"
#include "FPSGameplayTags.h"
#include "FPSNetDebug.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimInstance.h"
#include "Components/ShapeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UFPSWeaponComponent::UFPSWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // only the owner needs bloom decay; hero enables it
	SetIsReplicatedByDefault(true); // needed for RPCs on a component
}

void UFPSWeaponComponent::SetupMeshes(USkeletalMeshComponent* InArmsMesh, USkeletalMeshComponent* InWeaponFP, USkeletalMeshComponent* InWeaponTP)
{
	ArmsMesh = InArmsMesh;
	WeaponMeshFP = InWeaponFP;
	WeaponMeshTP = InWeaponTP;
}

// ---------------------------------------------------------------- Client: firing

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
	return !(ASC && (ASC->HasMatchingGameplayTag(FPSTags::State_Dead) || ASC->HasMatchingGameplayTag(FPSTags::State_Stunned)));
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

	FVector EyeLocation;
	FRotator EyeRotation;
	PC->GetPlayerViewPoint(EyeLocation, EyeRotation);

	// 1. Direction: auto-test aims straight at the target, normal play uses camera + spread.
	FVector ShotDirection;
	FVector AimPoint;
	const bool bAutoTest = GetAutoTestAimPoint(AimPoint);
	if (bAutoTest)
	{
		ShotDirection = (AimPoint - EyeLocation).GetSafeNormal();
	}
	else
	{
		ShotDirection = FMath::VRandCone(EyeRotation.Vector(), FMath::DegreesToRadians(GetCurrentSpread()));
		CurrentBloom = FMath::Min(CurrentBloom + SpreadPerShot, MaxBloom);
		PC->SetControlRotation(PC->GetControlRotation() + FRotator(RecoilPitch, FMath::FRandRange(-RecoilYawRange, RecoilYawRange), 0.f));
	}

	// 2. Local trace: what the CLIENT sees. Used for visuals and accuracy stats only.
	const FVector TraceEnd = EyeLocation + ShotDirection * Range;
	FHitResult LocalHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FPSWeaponLocalTrace), false, Pawn);
	const bool bLocalHit = World->LineTraceSingleByChannel(LocalHit, EyeLocation, TraceEnd, ECC_Weapon, Params);
	const FVector VisualEnd = bLocalHit ? LocalHit.ImpactPoint : TraceEnd;

	AActor* LocalHitTarget = nullptr;
	if (bLocalHit && LocalHit.GetActor() && LocalHit.GetActor()->FindComponentByClass<UFPSLagCompensationComponent>())
	{
		LocalHitTarget = LocalHit.GetActor();
	}

	const int32 ShotId = ++NextShotId;
	if (PendingShots.Num() > 256)
	{
		PendingShots.Reset(); // results lost (e.g. huge lag), don't grow forever
	}
	PendingShots.Add(ShotId, LocalHitTarget);
	Stats.ShotsFired++;
	if (LocalHitTarget)
	{
		Stats.ClientHits++;
	}

	// 3. FX.
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
	if (FPSNet::IsDebugDrawEnabled())
	{
		DrawDebugLine(World, GetMuzzleLocation(true), VisualEnd, FColor::Green, false, 2.f, 0, 0.5f);

		// GREEN ghosts: where the client sees other players right now.
		TArray<FFPSDebugHitbox> ClientBoxes;
		UFPSLagCompensationComponent::GatherDebugHitboxes(World, Pawn, EyeLocation, TraceEnd, ClientBoxes);
		for (const FFPSDebugHitbox& Box : ClientBoxes)
		{
			UFPSLagCompensationComponent::DrawDebugHitbox(World, Box, FColor::Green, 2.f);
		}
	}

	// 4. Which server time are we looking at? Others reach us half a round trip late.
	double TargetTime = World->GetTimeSeconds();
	if (const UFPSLagCompensationComponent* Clock = Pawn->FindComponentByClass<UFPSLagCompensationComponent>())
	{
		TargetTime = Clock->GetServerTime() - Clock->GetRoundTripTime() * 0.5 - FPSNet::GetRewindOffsetSeconds();
	}

	ServerFire(EyeLocation, ShotDirection, TargetTime, ShotId);
}

bool UFPSWeaponComponent::GetAutoTestAimPoint(FVector& OutPoint) const
{
	const AActor* Target = AutoTestTarget.Get();
	const UFPSLagCompensationComponent* LagComp = Target ? Target->FindComponentByClass<UFPSLagCompensationComponent>() : nullptr;
	if (!LagComp)
	{
		return false;
	}

	// Aim at the body hitbox (first hitbox that isn't the head).
	for (const UShapeComponent* Hitbox : LagComp->GetHitboxes())
	{
		if (Hitbox && !Hitbox->ComponentHasTag(FPS_HEAD_HITBOX_TAG))
		{
			OutPoint = Hitbox->GetComponentLocation();
			return true;
		}
	}
	return false;
}

void UFPSWeaponComponent::SetAutoTestTarget(AActor* Target)
{
	AutoTestTarget = Target;
}

void UFPSWeaponComponent::ResetStats()
{
	Stats = FFPSShotStats();
	PendingShots.Reset();
}

// ---------------------------------------------------------------- Server

void UFPSWeaponComponent::ServerFire_Implementation(FVector_NetQuantize Start, FVector_NetQuantizeNormal Direction, double TargetTime, int32 ShotId)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	UWorld* World = GetWorld();
	if (!Pawn || !World || !CanFire())
	{
		return;
	}

	// Check 1: fire rate (with slack for jitter).
	const double Now = World->GetTimeSeconds();
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

	// REWIND: put every other pawn's hitboxes where the shooter saw them.
	// Not needed for the host's own shots (no latency).
	bool bRewound = false;
	float RewindMs = 0.f;
	if (FPSNet::IsRewindEnabled() && !Pawn->IsLocallyControlled())
	{
		// Clamp: never rewind more than MaxRewind (stops "I fired 2 seconds ago" cheats).
		const double RewindTime = FMath::Clamp(TargetTime, Now - FPSNet::GetMaxRewindSeconds(), Now);
		RewindMs = static_cast<float>((Now - RewindTime) * 1000.0);
		UFPSLagCompensationComponent::RewindAll(World, RewindTime, Pawn);
		bRewound = true;
	}

	const FVector End = Start + Direction * Range;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FPSWeaponServerTrace), false, Pawn);
	const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Weapon, Params);
	const FVector HitEnd = bHit ? Hit.ImpactPoint : End;

	// BLUE ghosts for the shooter: hitboxes where the server had them for this trace.
	TArray<FFPSDebugHitbox> DebugBoxes;
	if (FPSNet::IsDebugDrawEnabled())
	{
		UFPSLagCompensationComponent::GatherDebugHitboxes(World, Pawn, Start, End, DebugBoxes);
	}

	// ALWAYS restore before doing anything else.
	if (bRewound)
	{
		UFPSLagCompensationComponent::RestoreAll(World);
	}

	if (FPSNet::IsDebugDrawEnabled())
	{
		DrawDebugLine(World, Start, HitEnd, FColor::Red, false, 2.f, 0, 0.5f);
	}

	MulticastShotFX(HitEnd);

	// Did we hit something shootable?
	AActor* HitActor = bHit ? Hit.GetActor() : nullptr;
	if (HitActor && !HitActor->FindComponentByClass<UFPSLagCompensationComponent>())
	{
		HitActor = nullptr; // wall, floor...
	}

	bool bHeadshot = false;
	bool bKill = false;
	float Damage = 0.f;

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(HitActor);
	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
	const APawn* TargetPawn = Cast<APawn>(HitActor);
	const bool bFriendlyBlocked = !(bAllowFriendlyFire || FPSNet::IsFriendlyFireEnabled()) && TargetPawn && TargetPawn->IsPlayerControlled();

	if (TargetASC && SourceASC && !bFriendlyBlocked && !TargetASC->HasMatchingGameplayTag(FPSTags::State_Dead))
	{
		bHeadshot = Hit.GetComponent() && Hit.GetComponent()->ComponentHasTag(FPS_HEAD_HITBOX_TAG);
		Damage = BodyDamage * (bHeadshot ? HeadshotMultiplier : 1.f);

		if (!FPSNet::IsNoDamage())
		{
			UFPSDamageEffect::ApplyDamage(SourceASC, TargetASC, Damage, Pawn);
		}
		bKill = TargetASC->HasMatchingGameplayTag(FPSTags::State_Dead);

		UE_LOG(LogTemp, Verbose, TEXT("[Weapon] %s hit %s (%s) for %.0f%s | rewind %s %.0f ms"),
			*Pawn->GetName(), *GetNameSafe(HitActor), *GetNameSafe(Hit.GetComponent()), Damage,
			bKill ? TEXT(" KILL") : TEXT(""), bRewound ? TEXT("ON") : TEXT("OFF"), RewindMs);
	}

	ClientShotResult(ShotId, HitActor, bHeadshot, bKill, Damage, bRewound, RewindMs, DebugBoxes);
}

// ---------------------------------------------------------------- Client: results

void UFPSWeaponComponent::ClientShotResult_Implementation(int32 ShotId, AActor* HitActor, bool bHeadshot, bool bKill, float Damage,
	bool bRewound, float RewindMs, const TArray<FFPSDebugHitbox>& DebugBoxes)
{
	bLastShotRewound = bRewound;
	LastRewindMs = RewindMs;

	// Stats: compare with what our local trace saw for this shot.
	TWeakObjectPtr<AActor> ClientTarget;
	if (PendingShots.RemoveAndCopyValue(ShotId, ClientTarget))
	{
		if (ClientTarget.IsValid())
		{
			if (HitActor && HitActor == ClientTarget.Get())
			{
				Stats.ServerAgreed++;
			}
		}
		else if (HitActor)
		{
			Stats.PhantomHits++;
		}
	}

	if (FPSNet::IsDebugDrawEnabled())
	{
		for (const FFPSDebugHitbox& Box : DebugBoxes)
		{
			UFPSLagCompensationComponent::DrawDebugHitbox(GetWorld(), Box, FColor::Blue, 2.f);
		}
	}

	if (!HitActor)
	{
		return;
	}
	LastHitTime = GetWorld()->GetTimeSeconds();
	bLastHitHeadshot = bHeadshot;

	if (HitConfirmSound)
	{
		UGameplayStatics::PlaySound2D(this, HitConfirmSound);
	}
	if (GEngine && !IsAutoTesting())
	{
		const FString Msg = FString::Printf(TEXT("%s %.0f%s"), bHeadshot ? TEXT("HEADSHOT") : TEXT("HIT"), Damage, bKill ? TEXT("  KILL") : TEXT(""));
		GEngine->AddOnScreenDebugMessage(2000, 0.5f, bHeadshot ? FColor::Orange : FColor::White, Msg);
	}
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
	if (FPSNet::IsDebugDrawEnabled())
	{
		DrawDebugLine(GetWorld(), Muzzle, TraceEnd, FColor::Yellow, false, 1.f, 0, 0.5f);
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
	CurrentBloom = FMath::FInterpConstantTo(CurrentBloom, 0.f, DeltaTime, BloomRecoveryRate);
}
