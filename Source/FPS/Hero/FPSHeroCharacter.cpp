#include "FPSHeroCharacter.h"
#include "FPSPlayerState.h"
#include "FPSAttributeSet.h"
#include "FPSGameplayAbility.h"
#include "FPSInputConfig.h"
#include "FPSDamageEffect.h"
#include "FPSGameplayTags.h"
#include "FPSWeaponComponent.h"
#include "FPSCollision.h"
#include "FPSLagCompensationComponent.h"
#include "FPSNetDebug.h"
#include "FPSEnemyCharacter.h"
#include "FPSEnemySpawner.h"
#include "FPSGameState.h"
#include "FPSWaveManager.h"
#include "Engine/NetConnection.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/SphereComponent.h"

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

	// Same first-person setup as the 5.6 template: full-body FP mesh, camera on its head bone.
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonMesh->bCastDynamicShadow = false;
	FirstPersonMesh->CastShadow = false;
	FirstPersonMesh->SetCollisionProfileName(TEXT("NoCollision"));

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(FirstPersonMesh, FName(TEXT("head")));
	FirstPersonCamera->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.f), FRotator(0.f, 90.f, -90.f));
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->bEnableFirstPersonFieldOfView = true;
	FirstPersonCamera->bEnableFirstPersonScale = true;
	FirstPersonCamera->FirstPersonFieldOfView = 70.f;
	FirstPersonCamera->FirstPersonScale = 0.6f;

	// Third-person body: others see it, owner doesn't.
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
	// Server must update bones even when it can't see the mesh, or the head hitbox won't follow the animation.
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	// Rifle meshes: FP one for the owner, TP one for everyone else.
	WeaponMeshFP = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMeshFP"));
	WeaponMeshFP->SetupAttachment(FirstPersonMesh, FName(TEXT("HandGrip_R")));
	WeaponMeshFP->SetOnlyOwnerSee(true);
	WeaponMeshFP->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	WeaponMeshFP->CastShadow = false;
	WeaponMeshFP->SetCollisionProfileName(TEXT("NoCollision"));

	WeaponMeshTP = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMeshTP"));
	WeaponMeshTP->SetupAttachment(GetMesh(), FName(TEXT("HandGrip_R")));
	WeaponMeshTP->SetOwnerNoSee(true);
	WeaponMeshTP->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
	WeaponMeshTP->SetCollisionProfileName(TEXT("NoCollision"));

	WeaponComponent = CreateDefaultSubobject<UFPSWeaponComponent>(TEXT("WeaponComponent"));
	LagCompensationComponent = CreateDefaultSubobject<UFPSLagCompensationComponent>(TEXT("LagCompensation"));

	// Hitboxes: only the Weapon trace channel hits these. Week 3 rewinds them.
	HeadHitbox = CreateDefaultSubobject<USphereComponent>(TEXT("HeadHitbox"));
	HeadHitbox->SetupAttachment(GetMesh(), FName(TEXT("head")));
	HeadHitbox->InitSphereRadius(14.f);
	HeadHitbox->ComponentTags.Add(FPS_HEAD_HITBOX_TAG);
	ConfigureHitbox(HeadHitbox);

	BodyHitbox = CreateDefaultSubobject<UCapsuleComponent>(TEXT("BodyHitbox"));
	BodyHitbox->SetupAttachment(GetCapsuleComponent());
	BodyHitbox->InitCapsuleSize(28.f, 70.f);
	BodyHitbox->SetRelativeLocation(FVector(0.f, 0.f, -22.f));
	ConfigureHitbox(BodyHitbox);

	// Movement capsule and body mesh must NOT stop bullets, only hitboxes do.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Weapon, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Weapon, ECR_Ignore);

	bUseControllerRotationYaw = true;
}

void AFPSHeroCharacter::ConfigureHitbox(UShapeComponent* Hitbox)
{
	Hitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Hitbox->SetCollisionObjectType(ECC_WorldDynamic);
	Hitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
	Hitbox->SetCollisionResponseToChannel(ECC_Weapon, ECR_Block);
	Hitbox->SetGenerateOverlapEvents(false);
	Hitbox->CanCharacterStepUpOn = ECB_No;
	Hitbox->SetHiddenInGame(true);
}

void AFPSHeroCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	WeaponComponent->SetupMeshes(FirstPersonMesh, WeaponMeshFP, WeaponMeshTP);
}

void AFPSHeroCharacter::BeginPlay()
{
	Super::BeginPlay();

	BaseFov = FirstPersonCamera->FieldOfView;

	// Hide head (and its children) on the FP mesh only. Camera socket still works.
	for (const FName& Bone : FirstPersonHiddenBones)
	{
		if (FirstPersonMesh->GetBoneIndex(Bone) != INDEX_NONE)
		{
			FirstPersonMesh->HideBoneByName(Bone, EPhysBodyOp::PBO_None);
		}
	}
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

	// This is OUR pawn: turn on the ticks only the owner needs.
	WeaponComponent->SetComponentTickEnabled(true);
	LagCompensationComponent->SetComponentTickEnabled(true);

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	// Menus leave UI input mode / cursor on. In game: capture mouse, hide cursor.
	PC->SetInputMode(FInputModeGameOnly());
	PC->SetShowMouseCursor(false);

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		if (DefaultMappingContext)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
		for (UInputMappingContext* Context : AdditionalMappingContexts)
		{
			if (Context)
			{
				Subsystem->AddMappingContext(Context, 0);
			}
		}
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
	if (MouseLookAction) EIC->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ThisClass::Look);
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
	if (IsStunned())
	{
		return;
	}
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
	DOREPLIFETIME_CONDITION(AFPSHeroCharacter, RespawnServerTime, COND_OwnerOnly);
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

	if (const AGameStateBase* GS = GetWorld()->GetGameState())
	{
		RespawnServerTime = static_cast<float>(GS->GetServerWorldTimeSeconds()) + RespawnDelay;
	}
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
	FirstPersonMesh->SetVisibility(false, true);
	HeadHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponComponent->StopFiring();

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
	// Match over (defeat/victory): stay dead, the map reloads soon.
	if (const AFPSGameState* GS = GetWorld()->GetGameState<AFPSGameState>())
	{
		if (GS->IsMatchOver())
		{
			return;
		}
	}

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

	UFPSDamageEffect::ApplyDamage(SourceASC, TargetASC, Amount, this);
}

void AFPSHeroCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsLocallyControlled())
	{
		return;
	}

	TickDebugStrafe(DeltaSeconds);

	// FOV punch eases back to normal.
	if (FovKickCurrent > 0.01f)
	{
		FovKickCurrent = FMath::FInterpTo(FovKickCurrent, 0.f, DeltaSeconds, 8.f);
		FirstPersonCamera->SetFieldOfView(BaseFov + FovKickCurrent);
	}

#if !UE_BUILD_SHIPPING
	if (bShowDebugReadout && GEngine)
	{
		DrawDebugReadout();
	}
#endif
}

void AFPSHeroCharacter::DrawDebugReadout()
{
	int32 Line = 0;

	// Every hero's HP/shield as this client sees it (proves replication).
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

	// Enemies alive, as this client sees them (hidden = in the pool).
	int32 EnemiesAlive = 0;
	for (TActorIterator<AFPSEnemyCharacter> It(GetWorld()); It; ++It)
	{
		if (!It->IsHidden() && !It->IsCombatantDead())
		{
			++EnemiesAlive;
		}
	}
	GEngine->AddOnScreenDebugMessage(1000 + Line++, 0.f, FColor::Orange, FString::Printf(TEXT("ENEMIES alive %d"), EnemiesAlive));

	// Network: our measured round trip, engine ping, packet loss.
	const float RttMs = LagCompensationComponent->GetRoundTripTime() * 1000.f;
	const float PingMs = GetPlayerState() ? GetPlayerState()->GetPingInMilliseconds() : 0.f;
	float LossIn = 0.f;
	float LossOut = 0.f;
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (const UNetConnection* Conn = PC->GetNetConnection())
		{
			LossIn = Conn->GetInLossPercentage().GetAvgLossPercentage() * 100.f;
			LossOut = Conn->GetOutLossPercentage().GetAvgLossPercentage() * 100.f;
		}
	}
	GEngine->AddOnScreenDebugMessage(1000 + Line++, 0.f, FColor::Cyan,
		FString::Printf(TEXT("NET  RTT %.0f ms | Ping %.0f ms | Loss in %.0f%% out %.0f%%"), RttMs, PingMs, LossIn, LossOut));

	// Hit stats: did the server agree with what we saw?
	const FFPSShotStats& Stats = WeaponComponent->GetStats();
	GEngine->AddOnScreenDebugMessage(1000 + Line++, 0.f, FColor::Cyan,
		FString::Printf(TEXT("SHOTS %d | Client hits %d | Server agreed %d (%.1f%%) | Phantom %d | Rewind %s %.0f ms%s"),
			Stats.ShotsFired, Stats.ClientHits, Stats.ServerAgreed, Stats.GetAgreementPercent(), Stats.PhantomHits,
			WeaponComponent->WasLastShotRewound() ? TEXT("ON") : TEXT("OFF"), WeaponComponent->GetLastRewindMs(),
			WeaponComponent->IsAutoTesting() ? TEXT(" | AUTO TEST") : TEXT("")));
}

void AFPSHeroCharacter::TickDebugStrafe(float DeltaSeconds)
{
	if (!bDebugStrafe || bIsDead)
	{
		return;
	}

	StrafeTimer += DeltaSeconds;
	if (StrafeTimer >= 1.f)
	{
		StrafeTimer = 0.f;
		StrafeDirection *= -1.f;
	}
	AddMovementInput(GetActorRightVector(), StrafeDirection);
}

void AFPSHeroCharacter::DebugAutoTest(int32 bEnable)
{
	if (!bEnable)
	{
		WeaponComponent->SetAutoTestTarget(nullptr);
		WeaponComponent->StopFiring();
		return;
	}

	// Nearest other living hero.
	AFPSHeroCharacter* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (TActorIterator<AFPSHeroCharacter> It(GetWorld()); It; ++It)
	{
		if (*It == this || It->IsDead())
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(It->GetActorLocation(), GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = *It;
		}
	}

	if (!Best)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Hero] DebugAutoTest: no other hero found"));
		return;
	}

	WeaponComponent->ResetStats();
	WeaponComponent->SetAutoTestTarget(Best);
	WeaponComponent->StartFiring();
}

void AFPSHeroCharacter::DebugStrafe(int32 bEnable)
{
	bDebugStrafe = bEnable != 0;
	StrafeTimer = 0.f;
}

void AFPSHeroCharacter::DebugResetStats()
{
	WeaponComponent->ResetStats();
}

void AFPSHeroCharacter::DebugSpawnEnemies(int32 Count)
{
	ServerDebugEnemies(FMath::Clamp(Count, 0, 100), false);
}

void AFPSHeroCharacter::DebugKillEnemies()
{
	ServerDebugEnemies(0, true);
}

void AFPSHeroCharacter::DebugSkipWave()
{
	ServerDebugSkipWave();
}

void AFPSHeroCharacter::ServerDebugSkipWave_Implementation()
{
	for (TActorIterator<AFPSWaveManager> It(GetWorld()); It; ++It)
	{
		It->SkipWave();
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("[Hero] No FPSWaveManager in the map"));
}

void AFPSHeroCharacter::ServerDebugEnemies_Implementation(int32 SpawnCount, bool bKillAll)
{
	AFPSEnemySpawner* Spawner = nullptr;
	for (TActorIterator<AFPSEnemySpawner> It(GetWorld()); It; ++It)
	{
		Spawner = *It;
		break;
	}
	if (!Spawner)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Hero] No FPSEnemySpawner in the map"));
		return;
	}

	if (bKillAll)
	{
		Spawner->KillAll();
	}
	else
	{
		Spawner->SpawnMixed(FMath::Clamp(SpawnCount, 0, 100));
	}
}

// ---------------------------------------------------------------- Stun + ability feel

bool AFPSHeroCharacter::IsStunned() const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	return ASC && ASC->HasMatchingGameplayTag(FPSTags::State_Stunned);
}

bool AFPSHeroCharacter::CanJumpInternal_Implementation() const
{
	return Super::CanJumpInternal_Implementation() && !IsStunned();
}

void AFPSHeroCharacter::AddFovKick(float Degrees)
{
	FovKickCurrent = FMath::Max(FovKickCurrent, Degrees);
	FirstPersonCamera->SetFieldOfView(BaseFov + FovKickCurrent);
}

void AFPSHeroCharacter::PlayAbilityFX(USoundBase* Sound, UNiagaraSystem* VFX, bool bAttach)
{
	const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
	}
	if (VFX)
	{
		if (bAttach)
		{
			UNiagaraFunctionLibrary::SpawnSystemAttached(VFX, GetRootComponent(), NAME_None,
				FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
		}
		else
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, VFX, Feet);
		}
	}
}

void AFPSHeroCharacter::MulticastAbilityFX_Implementation(USoundBase* Sound, UNiagaraSystem* VFX, bool bAttach)
{
	if (IsLocallyControlled())
	{
		return; // owner already played it the moment they pressed the key
	}
	PlayAbilityFX(Sound, VFX, bAttach);
}
