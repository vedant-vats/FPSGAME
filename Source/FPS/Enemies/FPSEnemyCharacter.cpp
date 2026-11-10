#include "FPSEnemyCharacter.h"
#include "GAS/FPSAttributeSet.h"
#include "Core/FPSCollision.h"
#include "GAS/FPSDamageEffect.h"
#include "Enemies/FPSEnemyAIController.h"
#include "Core/FPSGameplayTags.h"
#include "Netcode/FPSLagCompensationComponent.h"
#include "Hero/FPSHeroCharacter.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AFPSEnemyCharacter::AFPSEnemyCharacter()
{
	// No actor tick normally. Movement ticks itself, AI thinks on a timer.
	// Tick is switched on ONLY during a charge rush (~1 s every 6 s, tanks only).
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// GAS: lives on the enemy itself. MINIMAL = no gameplay effects sent to clients, only tags + attributes.
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	AttributeSet = CreateDefaultSubobject<UFPSAttributeSet>(TEXT("AttributeSet"));

	LagCompensationComponent = CreateDefaultSubobject<UFPSLagCompensationComponent>(TEXT("LagCompensation"));

	// Hitboxes on the capsule (not bones). Fixed height = no animation needed for hit detection.
	HeadHitbox = CreateDefaultSubobject<USphereComponent>(TEXT("HeadHitbox"));
	HeadHitbox->SetupAttachment(GetCapsuleComponent());
	HeadHitbox->InitSphereRadius(14.f);
	HeadHitbox->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	HeadHitbox->ComponentTags.Add(FPS_HEAD_HITBOX_TAG);
	ConfigureHitbox(HeadHitbox);

	BodyHitbox = CreateDefaultSubobject<UCapsuleComponent>(TEXT("BodyHitbox"));
	BodyHitbox->SetupAttachment(GetCapsuleComponent());
	BodyHitbox->InitCapsuleSize(30.f, 60.f);
	BodyHitbox->SetRelativeLocation(FVector(0.f, 0.f, -20.f));
	ConfigureHitbox(BodyHitbox);

	// Bullets only hit hitboxes.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Weapon, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionProfileName(TEXT("NoCollision"));

	// Movement: NavMesh walking = no floor sweeps every frame (much cheaper for many AI).
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->DefaultLandMovementMode = MOVE_NavWalking;
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 540.f, 0.f);
	// Path following sets velocity directly by default (acceleration stays 0 on the server),
	// so anim BPs that check acceleration show idle while the AI moves. Use acceleration instead.
	if (FNavMovementProperties* NavProps = Movement->GetNavMovementProperties())
	{
		NavProps->bUseAccelerationForPaths = true;
	}
	bUseControllerRotationYaw = false;

	AIControllerClass = AFPSEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AFPSEnemyCharacter::ConfigureHitbox(UShapeComponent* Hitbox)
{
	Hitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Hitbox->SetCollisionObjectType(ECC_WorldDynamic);
	Hitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
	Hitbox->SetCollisionResponseToChannel(ECC_Weapon, ECR_Block);
	Hitbox->SetGenerateOverlapEvents(false);
	Hitbox->CanCharacterStepUpOn = ECB_No;
	Hitbox->SetHiddenInGame(true);
}

UAbilitySystemComponent* AFPSEnemyCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AFPSEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
	DefaultMaxAcceleration = GetCharacterMovement()->MaxAcceleration;

	if (HasAuthority())
	{
		// React to stun the moment it lands (don't wait for the next AI think).
		AbilitySystemComponent->RegisterGameplayTagEvent(FPSTags::State_Stunned, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::OnStunTagChanged);
	}
}

void AFPSEnemyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFPSEnemyCharacter, bIsDead);
	DOREPLIFETIME(AFPSEnemyCharacter, Tier);
}

// ---------------------------------------------------------------- Pool

void AFPSEnemyCharacter::Activate(const FVector& FloorLocation, const FRotator& Rotation, uint8 InTier)
{
	// Wake up on the network first, so the changes below get sent.
	SetNetDormancy(DORM_Awake);

	GetWorldTimerManager().ClearTimer(CorpseTimer);
	bActive = true;
	bIsDead = false;

	// Charge: clean state, and no charge in the first few seconds out of the gate.
	ChargeState = EFPSChargeState::None;
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
	NextChargeTime = GetWorld()->GetTimeSeconds() + ChargeCooldown * 0.5f;

	// Size first, then stand the (possibly bigger) capsule on the floor point.
	Tier = InTier;
	ApplyTierScale();
	const FVector Location = FloorLocation + FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);

	// Fresh stats: full health, no shield, no leftover tags or stun.
	AbilitySystemComponent->RemoveLooseGameplayTag(FPSTags::State_Dead, 99);
	AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(FPSTags::State_Stunned));
	AttributeSet->ResetAttributes(MaxHealth * GetTierHealthMultiplier(), 0.f);

	ResetVisuals();
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetHitboxesEnabled(true);
	GetCharacterMovement()->SetMovementMode(MOVE_NavWalking);

	// Don't blend hitbox history from the pool position to here.
	LagCompensationComponent->ClearHistory();
	LagCompensationComponent->SetComponentTickEnabled(true);

	if (AFPSEnemyAIController* AI = GetController<AFPSEnemyAIController>())
	{
		AI->StartThinking();
	}

	ForceNetUpdate();
}

void AFPSEnemyCharacter::Deactivate()
{
	bActive = false;
	EndCharge();
	ClearWallStun();
	GetWorldTimerManager().ClearTimer(AttackTimer);
	GetWorldTimerManager().ClearTimer(CorpseTimer);

	if (AFPSEnemyAIController* AI = GetController<AFPSEnemyAIController>())
	{
		AI->StopThinking();
	}

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetHitboxesEnabled(false);
	LagCompensationComponent->SetComponentTickEnabled(false);

	// Send "hidden" once, then stop replicating this actor until it's used again.
	ForceNetUpdate();
	SetNetDormancy(DORM_DormantAll);
}

void AFPSEnemyCharacter::SetHitboxesEnabled(bool bEnabled)
{
	const ECollisionEnabled::Type Mode = bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision;
	HeadHitbox->SetCollisionEnabled(Mode);
	BodyHitbox->SetCollisionEnabled(Mode);
}

// ---------------------------------------------------------------- Death

void AFPSEnemyCharacter::OnCombatantDied()
{
	if (!HasAuthority() || bIsDead)
	{
		return;
	}

	bIsDead = true;
	AbilitySystemComponent->CancelAllAbilities();
	AbilitySystemComponent->AddLooseGameplayTag(FPSTags::State_Dead);
	GetWorldTimerManager().ClearTimer(AttackTimer);
	EndCharge();
	ClearWallStun();

	if (AFPSEnemyAIController* AI = GetController<AFPSEnemyAIController>())
	{
		AI->StopThinking();
	}

	ApplyDeathVisuals(); // OnRep doesn't run on the server
	OnEnemyDied.Broadcast(this);

	GetWorldTimerManager().SetTimer(CorpseTimer, this, &ThisClass::Deactivate, CorpseTime, false);
}

void AFPSEnemyCharacter::OnRep_IsDead()
{
	if (bIsDead)
	{
		ApplyDeathVisuals();
	}
	else
	{
		ResetVisuals();
	}
}

void AFPSEnemyCharacter::ApplyDeathVisuals()
{
	SetHitboxesEnabled(false);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	GetMesh()->SetSimulatePhysics(true);

	if (DeathSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DeathSound, GetActorLocation());
	}
}

void AFPSEnemyCharacter::ResetVisuals()
{
	// Undo the ragdoll: physics off, mesh back on the capsule where the BP placed it.
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (MeshComp->IsSimulatingPhysics())
	{
		MeshComp->SetSimulatePhysics(false);
		MeshComp->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		MeshComp->SetRelativeLocationAndRotation(GetBaseTranslationOffset(), GetBaseRotationOffset());
	}
	MeshComp->SetCollisionProfileName(TEXT("NoCollision"));
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

// ---------------------------------------------------------------- Combat

bool AFPSEnemyCharacter::IsStunned() const
{
	return AbilitySystemComponent->HasMatchingGameplayTag(FPSTags::State_Stunned);
}

bool AFPSEnemyCharacter::CanAttack() const
{
	return bActive && !bIsDead && !IsStunned() && GetWorld()->GetTimeSeconds() >= NextAttackTime;
}

void AFPSEnemyCharacter::StartAttack(AActor* Target)
{
	if (!Target || !CanAttack())
	{
		return;
	}

	AttackTarget = Target;
	NextAttackTime = GetWorld()->GetTimeSeconds() + AttackCooldown;

	// Face the target for the swing.
	FRotator Face = (Target->GetActorLocation() - GetActorLocation()).Rotation();
	Face.Pitch = 0.f;
	Face.Roll = 0.f;
	SetActorRotation(Face);

	MulticastPlayAttack();
	GetWorldTimerManager().SetTimer(AttackTimer, this, &ThisClass::FinishAttack, AttackWindup, false);
}

void AFPSEnemyCharacter::FinishAttack()
{
	AActor* Target = AttackTarget.Get();
	if (!Target || !bActive || bIsDead || IsStunned())
	{
		return;
	}

	// Dodge window: if the player moved out of reach during the windup, it misses.
	if (FVector::Dist2D(Target->GetActorLocation(), GetActorLocation()) > AttackRange * 1.3f)
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (TargetASC && !TargetASC->HasMatchingGameplayTag(FPSTags::State_Dead))
	{
		UFPSDamageEffect::ApplyDamage(AbilitySystemComponent, TargetASC, AttackDamage * GetTierDamageMultiplier(), this);
	}
}

void AFPSEnemyCharacter::MulticastPlayAttack_Implementation()
{
	if (AttackMontage)
	{
		if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
		{
			Anim->Montage_Play(AttackMontage);
		}
	}
}

void AFPSEnemyCharacter::OnStunTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0)
	{
		// Stunned: stop now and cancel the swing (and any charge) in progress.
		GetWorldTimerManager().ClearTimer(AttackTimer);
		EndCharge();
		if (AFPSEnemyAIController* AI = GetController<AFPSEnemyAIController>())
		{
			AI->StopMovement();
		}
	}
	// Stun ended: the AI picks up on its next think.
}

// ---------------------------------------------------------------- Charge

bool AFPSEnemyCharacter::CanStartCharge(const AActor* Target) const
{
	if (!bCanCharge || !Target || ChargeState != EFPSChargeState::None || !bActive || bIsDead || IsStunned()
		|| GetWorld()->GetTimeSeconds() < NextChargeTime)
	{
		return false;
	}

	const float Distance = FVector::Dist2D(Target->GetActorLocation(), GetActorLocation());
	if (Distance < ChargeMinRange || Distance > ChargeMaxRange)
	{
		return false;
	}

	// Line of sight: never charge through a wall. One trace, only when everything else passed.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemyChargeLOS), false, this);
	Params.AddIgnoredActor(Target);
	return !GetWorld()->LineTraceTestByChannel(GetActorLocation(), Target->GetActorLocation(), ECC_Visibility, Params);
}

void AFPSEnemyCharacter::StartCharge(AActor* Target)
{
	if (!CanStartCharge(Target))
	{
		return;
	}

	ChargeState = EFPSChargeState::Windup;
	ChargeTarget = Target;

	FRotator Face = (Target->GetActorLocation() - GetActorLocation()).Rotation();
	Face.Pitch = 0.f;
	Face.Roll = 0.f;
	SetActorRotation(Face);

	MulticastChargeWindup();
	GetWorldTimerManager().SetTimer(ChargeTimer, this, &ThisClass::BeginChargeRun, ChargeWindup, false);
}

void AFPSEnemyCharacter::BeginChargeRun()
{
	AActor* Target = ChargeTarget.Get();
	if (!bActive || bIsDead || IsStunned() || !Target)
	{
		EndCharge();
		return;
	}

	// Commit: aim at where the player is NOW and run past it. No homing = a sidestep or Dash beats it.
	FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	ToTarget.Z = 0.f;
	ChargeDirection = ToTarget.GetSafeNormal();
	ChargeEndPoint = Target->GetActorLocation() + ChargeDirection * ChargeOvershoot;
	ChargeElapsed = 0.f;
	ChargeState = EFPSChargeState::Running;

	// Path following off; the rush drives the movement itself.
	if (AFPSEnemyAIController* AI = GetController<AFPSEnemyAIController>())
	{
		AI->StopMovement();
	}

	// Walking (not NavWalking) so walls and cover physically stop it -> wall stun.
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->SetMovementMode(MOVE_Walking);
	Movement->MaxWalkSpeed = ChargeSpeed;
	Movement->MaxAcceleration = 8000.f; // full speed almost instantly
	Movement->Velocity = ChargeDirection * ChargeSpeed;

	SetActorTickEnabled(true);
}

void AFPSEnemyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (ChargeState != EFPSChargeState::Running)
	{
		SetActorTickEnabled(false);
		return;
	}

	ChargeElapsed += DeltaSeconds;
	AddMovementInput(ChargeDirection, 1.f);

	// 1) Ran into a player? (max 4 heroes: a plain loop is the cheapest check)
	const FVector MyLocation = GetActorLocation();
	for (TActorIterator<AFPSHeroCharacter> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead()
			&& FVector::Dist2D(It->GetActorLocation(), MyLocation) <= AttackRange
			&& FMath::Abs(It->GetActorLocation().Z - MyLocation.Z) < 200.f)
		{
			ChargeHitPlayer(*It);
			return;
		}
	}

	// 2) Blocked by a wall / cover / another enemy: speed collapses. Short grace while it speeds up.
	if (ChargeElapsed > 0.15f && GetVelocity().Size2D() < ChargeSpeed * 0.25f)
	{
		ChargeHitWall();
		return;
	}

	// 3) Passed the end point, or safety timeout.
	if (ChargeElapsed >= ChargeDuration
		|| FVector::DotProduct(ChargeEndPoint - MyLocation, ChargeDirection) <= 0.f)
	{
		EndCharge();
	}
}

void AFPSEnemyCharacter::ChargeHitPlayer(AActor* Target)
{
	EndCharge();
	GetCharacterMovement()->StopMovementImmediately();

	// Instant hit (the windup was the warning). Same damage as the normal attack.
	NextAttackTime = GetWorld()->GetTimeSeconds() + AttackCooldown;
	MulticastPlayAttack();

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (TargetASC && !TargetASC->HasMatchingGameplayTag(FPSTags::State_Dead))
	{
		UFPSDamageEffect::ApplyDamage(AbilitySystemComponent, TargetASC, AttackDamage * GetTierDamageMultiplier(), this);
	}
}

void AFPSEnemyCharacter::ChargeHitWall()
{
	EndCharge();
	GetCharacterMovement()->StopMovementImmediately();

	// Stunned tag = the AI stands still (same path as Shockwave's stun). Server only, no replication needed.
	bWallStunned = true;
	AbilitySystemComponent->AddLooseGameplayTag(FPSTags::State_Stunned);
	GetWorldTimerManager().SetTimer(WallStunTimer, this, &ThisClass::ClearWallStun, WallStunDuration, false);
}

void AFPSEnemyCharacter::ClearWallStun()
{
	GetWorldTimerManager().ClearTimer(WallStunTimer);
	if (bWallStunned)
	{
		bWallStunned = false;
		AbilitySystemComponent->RemoveLooseGameplayTag(FPSTags::State_Stunned);
	}
}

void AFPSEnemyCharacter::EndCharge()
{
	if (ChargeState == EFPSChargeState::None)
	{
		return;
	}

	ChargeState = EFPSChargeState::None;
	ChargeTarget = nullptr;
	SetActorTickEnabled(false);
	GetWorldTimerManager().ClearTimer(ChargeTimer);

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = MoveSpeed;
	Movement->MaxAcceleration = DefaultMaxAcceleration;
	if (bActive && !bIsDead && Movement->MovementMode == MOVE_Walking)
	{
		Movement->SetMovementMode(MOVE_NavWalking); // back to cheap nav movement
	}

	NextChargeTime = GetWorld()->GetTimeSeconds() + ChargeCooldown;
}

void AFPSEnemyCharacter::MulticastChargeWindup_Implementation()
{
	OnChargeWindup();
}

// ---------------------------------------------------------------- Tiers

void AFPSEnemyCharacter::OnRep_Tier()
{
	ApplyTierScale();
}

void AFPSEnemyCharacter::ApplyTierScale()
{
	const float Scale = Tier == 2 ? BossScale : (Tier == 1 ? EliteScale : 1.f);
	SetActorScale3D(FVector(Scale)); // capsule, mesh and hitboxes all scale with it
}

float AFPSEnemyCharacter::GetTierHealthMultiplier() const
{
	return Tier == 2 ? BossHealthMultiplier : (Tier == 1 ? EliteHealthMultiplier : 1.f);
}

float AFPSEnemyCharacter::GetTierDamageMultiplier() const
{
	return Tier == 2 ? BossDamageMultiplier : (Tier == 1 ? EliteDamageMultiplier : 1.f);
}
