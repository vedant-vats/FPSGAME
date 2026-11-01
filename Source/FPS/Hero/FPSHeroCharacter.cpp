#include "FPSHeroCharacter.h"
#include "FPSPlayerState.h"
#include "FPSAttributeSet.h"
#include "FPSGameplayAbility.h"
#include "FPSInputConfig.h"
#include "FPSDamageEffect.h"
#include "FPSGameplayTags.h"

#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

AFPSHeroCharacter::AFPSHeroCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 96.f);

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
	FirstPersonCamera->bUsePawnControlRotation = true;

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(FirstPersonCamera);
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->bCastDynamicShadow = false;
	FirstPersonMesh->CastShadow = false;
	FirstPersonMesh->SetCollisionProfileName(TEXT("NoCollision"));

	// Third-person body: others see it, owner doesn't.
	GetMesh()->SetOwnerNoSee(true);

	bUseControllerRotationYaw = true;
}

// ---------------------------------------------------------------- GAS

UAbilitySystemComponent* AFPSHeroCharacter::GetAbilitySystemComponent() const
{
	const AFPSPlayerState* PS = GetPlayerState<AFPSPlayerState>();
	return PS ? PS->GetAbilitySystemComponent() : nullptr;
}

UFPSAttributeSet* AFPSHeroCharacter::GetAttributeSet() const
{
	const AFPSPlayerState* PS = GetPlayerState<AFPSPlayerState>();
	return PS ? PS->GetAttributeSet() : nullptr;
}

void AFPSHeroCharacter::InitAbilitySystem()
{
	AFPSPlayerState* PS = GetPlayerState<AFPSPlayerState>();
	if (!PS)
	{
		UE_LOG(LogTemp, Error, TEXT("[Hero] %s has no AFPSPlayerState. Set PlayerStateClass on the GameMode."), *GetName());
		return;
	}

	PS->GetAbilitySystemComponent()->InitAbilityActorInfo(PS, this);
}

void AFPSHeroCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Server.
	InitAbilitySystem();

	AFPSPlayerState* PS = GetPlayerState<AFPSPlayerState>();
	if (!PS)
	{
		return;
	}

	UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
	ASC->RemoveLooseGameplayTag(FPSTags::State_Dead, 99);
	PS->GetAttributeSet()->ResetAttributes(DefaultMaxHealth, DefaultMaxShield);

	if (!PS->bDefaultAbilitiesGiven)
	{
		for (const TSubclassOf<UFPSGameplayAbility>& AbilityClass : DefaultAbilities)
		{
			if (AbilityClass)
			{
				ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, PS));
			}
		}
		PS->bDefaultAbilitiesGiven = true;
	}
}

void AFPSHeroCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// Client.
	InitAbilitySystem();
}

// ---------------------------------------------------------------- Input

void AFPSHeroCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !DefaultMappingContext)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(DefaultMappingContext, 0);
	}
}

void AFPSHeroCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogTemp, Error, TEXT("[Hero] Not an EnhancedInputComponent. Check project input settings."));
		return;
	}

	if (MoveAction) EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::Move);
	if (LookAction) EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ThisClass::Look);
	if (JumpAction)
	{
		EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}

	if (InputConfig)
	{
		for (const FFPSAbilityInputAction& Entry : InputConfig->AbilityInputActions)
		{
			if (Entry.InputAction && Entry.InputTag.IsValid())
			{
				EIC->BindAction(Entry.InputAction, ETriggerEvent::Started, this, &ThisClass::AbilityInputPressed, Entry.InputTag);
				EIC->BindAction(Entry.InputAction, ETriggerEvent::Completed, this, &ThisClass::AbilityInputReleased, Entry.InputTag);
			}
		}
	}
}

void AFPSHeroCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddMovementInput(GetActorForwardVector(), Axis.Y);
	AddMovementInput(GetActorRightVector(), Axis.X);
}

void AFPSHeroCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void AFPSHeroCharacter::AbilityInputPressed(FGameplayTag InputTag)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		const UFPSGameplayAbility* Ability = Cast<UFPSGameplayAbility>(Spec.Ability);
		if (Ability && Ability->InputTag.MatchesTagExact(InputTag))
		{
			ASC->AbilitySpecInputPressed(Spec);
			if (!Spec.IsActive())
			{
				ASC->TryActivateAbility(Spec.Handle);
			}
		}
	}
}

void AFPSHeroCharacter::AbilityInputReleased(FGameplayTag InputTag)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		const UFPSGameplayAbility* Ability = Cast<UFPSGameplayAbility>(Spec.Ability);
		if (Ability && Ability->InputTag.MatchesTagExact(InputTag))
		{
			ASC->AbilitySpecInputReleased(Spec);
		}
	}
}

// ---------------------------------------------------------------- Death / respawn

void AFPSHeroCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFPSHeroCharacter, bIsDead);
}

void AFPSHeroCharacter::HandleDeath()
{
	if (!HasAuthority() || bIsDead)
	{
		return;
	}

	bIsDead = true;

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->CancelAllAbilities();
		ASC->AddLooseGameplayTag(FPSTags::State_Dead);
	}

	UE_LOG(LogTemp, Log, TEXT("[Hero] %s died. Respawn in %.1fs"), *GetName(), RespawnDelay);

	ApplyDeathState(); // OnRep doesn't fire on server.
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &ThisClass::Respawn, RespawnDelay, false);
}

void AFPSHeroCharacter::OnRep_IsDead()
{
	if (bIsDead)
	{
		ApplyDeathState();
	}
}

void AFPSHeroCharacter::ApplyDeathState()
{
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Ragdoll if the body mesh has a physics asset, otherwise it just freezes.
	GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	GetMesh()->SetSimulatePhysics(true);
	FirstPersonMesh->SetVisibility(false);

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (IsLocallyControlled())
		{
			DisableInput(PC);
		}
	}
}

void AFPSHeroCharacter::Respawn()
{
	AController* OldController = GetController();
	UWorld* World = GetWorld();

	if (OldController)
	{
		OldController->UnPossess();
	}
	Destroy();

	if (OldController && World)
	{
		if (AGameModeBase* GM = World->GetAuthGameMode())
		{
			GM->RestartPlayer(OldController);
		}
	}
}

// ---------------------------------------------------------------- Debug

void AFPSHeroCharacter::DebugDamageOthers(float Amount)
{
	ServerDebugDamage(true, Amount);
}

void AFPSHeroCharacter::DebugDamageSelf(float Amount)
{
	ServerDebugDamage(false, Amount);
}

void AFPSHeroCharacter::ServerDebugDamage_Implementation(bool bOthers, float Amount)
{
	Amount = FMath::Clamp(Amount, 0.f, 1000.f);

	if (!bOthers)
	{
		ApplyDamageTo(this, Amount);
		return;
	}

	for (TActorIterator<AFPSHeroCharacter> It(GetWorld()); It; ++It)
	{
		if (*It != this)
		{
			ApplyDamageTo(*It, Amount);
		}
	}
}

void AFPSHeroCharacter::ApplyDamageTo(AFPSHeroCharacter* Target, float Amount)
{
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponent();
	UAbilitySystemComponent* TargetASC = Target ? Target->GetAbilitySystemComponent() : nullptr;
	if (!SourceASC || !TargetASC || Target->IsDead())
	{
		return;
	}

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddSourceObject(this);

	FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(UFPSDamageEffect::StaticClass(), 1.f, Context);
	if (!Spec.IsValid())
	{
		return;
	}

	Spec.Data->SetSetByCallerMagnitude(UFPSDamageEffect::DamageSetByCallerName, Amount);
	SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
}

void AFPSHeroCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bShowDebugReadout || !IsLocallyControlled() || !GEngine)
	{
		return;
	}

	// Every hero's HP/shield as this client sees it (proves replication).
	int32 Line = 0;
	for (TActorIterator<AFPSHeroCharacter> It(GetWorld()); It; ++It)
	{
		const UFPSAttributeSet* AS = It->GetAttributeSet();
		if (!AS)
		{
			continue;
		}

		const APlayerState* PS = It->GetPlayerState();
		const FString Who = (*It == this) ? TEXT("YOU") : (PS ? PS->GetPlayerName() : It->GetName());
		const FString Msg = FString::Printf(TEXT("%s  HP %.0f/%.0f  SH %.0f/%.0f%s"),
			*Who, AS->GetHealth(), AS->GetMaxHealth(), AS->GetShield(), AS->GetMaxShield(),
			It->IsDead() ? TEXT("  [DEAD]") : TEXT(""));

		GEngine->AddOnScreenDebugMessage(1000 + Line++, 0.f, It->IsDead() ? FColor::Red : FColor::Green, Msg);
	}
}
