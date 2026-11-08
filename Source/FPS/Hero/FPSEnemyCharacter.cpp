#include "FPSEnemyCharacter.h"
#include "FPSAttributeSet.h"
#include "FPSCollision.h"
#include "FPSDamageEffect.h"
#include "FPSEnemyAIController.h"
#include "FPSGameplayTags.h"
#include "FPSLagCompensationComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AFPSEnemyCharacter::AFPSEnemyCharacter()
{
	// No actor tick at all. Movement ticks itself, AI thinks on a timer.
	PrimaryActorTick.bCanEverTick = false;

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
}

// ---------------------------------------------------------------- Pool

void AFPSEnemyCharacter::Activate(const FVector& Location, const FRotator& Rotation)
{
	// Wake up on the network first, so the changes below get sent.
	SetNetDormancy(DORM_Awake);

	GetWorldTimerManager().ClearTimer(CorpseTimer);
	bActive = true;
	bIsDead = false;

	SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);

	// Fresh stats: full health, no shield, no leftover tags or stun.
	AbilitySystemComponent->RemoveLooseGameplayTag(FPSTags::State_Dead, 99);
	AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(FPSTags::State_Stunned));
	AttributeSet->ResetAttributes(MaxHealth, 0.f);

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
		UFPSDamageEffect::ApplyDamage(AbilitySystemComponent, TargetASC, AttackDamage, this);
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
		// Stunned: stop now and cancel the swing in progress.
		GetWorldTimerManager().ClearTimer(AttackTimer);
		if (AFPSEnemyAIController* AI = GetController<AFPSEnemyAIController>())
		{
			AI->StopMovement();
		}
	}
	// Stun ended: the AI picks up on its next think.
}
