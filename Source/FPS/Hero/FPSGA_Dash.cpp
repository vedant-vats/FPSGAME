#include "FPSGA_Dash.h"
#include "FPSGameplayTags.h"
#include "FPSHeroCharacter.h"

#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

UFPSGA_Dash::UFPSGA_Dash()
{
	InputTag = FPSTags::InputTag_Ability_Dash;
	MaxCharges = 2;
	HUDName = NSLOCTEXT("FPS", "DashName", "DASH");
	HUDKey = NSLOCTEXT("FPS", "DashKey", "SHIFT");
}

void UFPSGA_Dash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AFPSHeroCharacter* Hero = Cast<AFPSHeroCharacter>(GetAvatarActorFromActorInfo());
	if (!Hero || !CommitAbility(Handle, ActorInfo, ActivationInfo)) // Commit = use a charge (applies cooldown GE)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Direction: where you're moving (WASD). Standing still = forward. Flat, no flying up.
	UCharacterMovementComponent* Movement = Hero->GetCharacterMovement();
	FVector Direction = Movement->GetCurrentAcceleration().GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = Hero->GetActorForwardVector().GetSafeNormal2D();
	}

	// Root motion: constant speed for DashDuration, no gravity (works in air),
	// then clamp speed so you don't fly off at 3000 cm/s.
	UAbilityTask_ApplyRootMotionConstantForce* Task = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, NAME_None, Direction, DashDistance / DashDuration, DashDuration,
		false, nullptr, ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector,
		Movement->GetMaxSpeed(), false);
	Task->OnFinish.AddDynamic(this, &ThisClass::OnDashFinished);
	Task->ReadyForActivation();

	// Feel: only the player who dashed gets the camera stuff.
	if (IsLocallyControlled())
	{
		Hero->AddFovKick(FovKick);
		if (CameraShake)
		{
			if (APlayerController* PC = Cast<APlayerController>(Hero->GetController()))
			{
				PC->PlayerCameraManager->StartCameraShake(CameraShake);
			}
		}
		Hero->PlayAbilityFX(DashSound, DashVFX, true);
	}

	// Everyone else sees the trail / hears the whoosh.
	if (HasAuthority(&ActivationInfo))
	{
		Hero->MulticastAbilityFX(DashSound, DashVFX, true);
	}
}

void UFPSGA_Dash::OnDashFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
