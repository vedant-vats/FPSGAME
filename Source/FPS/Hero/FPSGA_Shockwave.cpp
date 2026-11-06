#include "FPSGA_Shockwave.h"
#include "FPSDamageEffect.h"
#include "FPSGameplayTags.h"
#include "FPSHeroCharacter.h"
#include "FPSNetDebug.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Camera/PlayerCameraManager.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

UFPSGA_Shockwave::UFPSGA_Shockwave()
{
	InputTag = FPSTags::InputTag_Ability_Area;
	HUDName = NSLOCTEXT("FPS", "ShockwaveName", "SHOCKWAVE");
	HUDKey = NSLOCTEXT("FPS", "ShockwaveKey", "E");
}

void UFPSGA_Shockwave::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AFPSHeroCharacter* Hero = Cast<AFPSHeroCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Feel right away on the player who pressed E (no waiting for the server).
	if (IsLocallyControlled())
	{
		if (CameraShake)
		{
			if (APlayerController* PC = Cast<APlayerController>(Hero->GetController()))
			{
				PC->PlayerCameraManager->StartCameraShake(CameraShake);
			}
		}
		Hero->PlayAbilityFX(ShockwaveSound, ShockwaveVFX, false);
	}

	// Server decides who got hit.
	if (HasAuthority(&ActivationInfo))
	{
		ApplyShockwave(Hero);
		Hero->MulticastAbilityFX(ShockwaveSound, ShockwaveVFX, false);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UFPSGA_Shockwave::ApplyShockwave(AActor* Source) const
{
	UWorld* World = Source->GetWorld();
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	const FVector Center = Source->GetActorLocation();

	// 1. Every pawn capsule inside the radius.
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FPSShockwave), false, Source);
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Radius), Params);

	TSet<AActor*> Done; // a pawn can overlap with more than one component
	for (const FOverlapResult& Overlap : Overlaps)
	{
		APawn* Target = Cast<APawn>(Overlap.GetActor());
		if (!Target || Done.Contains(Target))
		{
			continue;
		}
		Done.Add(Target);

		if (!bAffectPlayers && Target->IsPlayerControlled())
		{
			continue;
		}

		UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
		if (!TargetASC || TargetASC->HasMatchingGameplayTag(FPSTags::State_Dead))
		{
			continue;
		}

		// 2. No hitting through walls.
		FHitResult WallHit;
		FCollisionQueryParams LosParams(SCENE_QUERY_STAT(FPSShockwaveLOS), false, Source);
		LosParams.AddIgnoredActor(Target);
		if (World->LineTraceSingleByChannel(WallHit, Center, Target->GetActorLocation(), ECC_Visibility, LosParams))
		{
			continue;
		}

		// 3. Damage + stun through GAS.
		UFPSDamageEffect::ApplyDamage(SourceASC, TargetASC, Damage, Source);
		if (StunEffect && !TargetASC->HasMatchingGameplayTag(FPSTags::State_Dead))
		{
			const FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
			const FGameplayEffectSpecHandle StunSpec = SourceASC->MakeOutgoingSpec(StunEffect, 1.f, Context);
			if (StunSpec.IsValid())
			{
				SourceASC->ApplyGameplayEffectSpecToTarget(*StunSpec.Data.Get(), TargetASC);
			}
		}

		// 4. Knockback away from the center.
		ACharacter* TargetCharacter = Cast<ACharacter>(Target);
		if (TargetCharacter && (bKnockbackPlayers || !Target->IsPlayerControlled()))
		{
			const FVector Away = (Target->GetActorLocation() - Center).GetSafeNormal2D();
			TargetCharacter->LaunchCharacter(Away * KnockbackStrength + FVector(0.f, 0.f, KnockbackUp), true, true);
		}

		UE_LOG(LogTemp, Verbose, TEXT("[Shockwave] %s hit %s"), *Source->GetName(), *Target->GetName());
	}

	if (FPSNet::IsDebugDrawEnabled())
	{
		DrawDebugSphere(World, Center, Radius, 24, FColor::Orange, false, 0.5f);
	}
}
