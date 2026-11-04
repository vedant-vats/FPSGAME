#include "FPSLagCompensationComponent.h"
#include "FPSCollision.h"

#include "Components/CapsuleComponent.h"
#include "Components/ShapeComponent.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"

UFPSLagCompensationComponent::UFPSLagCompensationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Record AFTER animation has updated bones this frame, so the head hitbox is current.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetIsReplicatedByDefault(true); // needed for the clock RPCs

	TimeSinceSync = 1000.f; // sync right away on first tick
}

void UFPSLagCompensationComponent::BeginPlay()
{
	Super::BeginPlay();
	CollectHitboxes();
}

void UFPSLagCompensationComponent::CollectHitboxes()
{
	Hitboxes.Reset();

	TArray<UShapeComponent*> Shapes;
	GetOwner()->GetComponents<UShapeComponent>(Shapes);
	for (UShapeComponent* Shape : Shapes)
	{
		// Hitbox = blocks the Weapon channel. The movement capsule ignores it, so it's skipped.
		if (Shape && Shape->GetCollisionResponseToChannel(ECC_Weapon) == ECR_Block)
		{
			Hitboxes.Add(Shape);
		}
	}
}

void UFPSLagCompensationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AActor* Owner = GetOwner();
	if (Owner->HasAuthority())
	{
		RecordFrame();
	}

	// Clock sync: only the owning client of a remote pawn needs it.
	const APawn* Pawn = Cast<APawn>(Owner);
	if (Pawn && Pawn->IsLocallyControlled() && !Owner->HasAuthority())
	{
		TimeSinceSync += DeltaTime;
		if (TimeSinceSync >= ClockSyncInterval)
		{
			TimeSinceSync = 0.f;
			ServerRequestTime(GetWorld()->GetTimeSeconds());
		}
	}
}

// ---------------------------------------------------------------- Clock

void UFPSLagCompensationComponent::ServerRequestTime_Implementation(double ClientSendTime)
{
	ClientReceiveTime(ClientSendTime, GetWorld()->GetTimeSeconds());
}

void UFPSLagCompensationComponent::ClientReceiveTime_Implementation(double ClientSendTime, double ServerTime)
{
	const double Now = GetWorld()->GetTimeSeconds();
	const float NewRoundTrip = static_cast<float>(Now - ClientSendTime);

	// Server sent ServerTime half a round trip ago, so "server now" is ServerTime + RTT/2.
	const double NewOffset = ServerTime + NewRoundTrip * 0.5 - Now;

	if (!bHasClockSync)
	{
		RoundTripTime = NewRoundTrip;
		ServerTimeOffset = NewOffset;
		bHasClockSync = true;
	}
	else
	{
		// Smooth out jitter.
		RoundTripTime = FMath::Lerp(RoundTripTime, NewRoundTrip, 0.2f);
		ServerTimeOffset = FMath::Lerp(ServerTimeOffset, NewOffset, 0.2);
	}
}

double UFPSLagCompensationComponent::GetServerTime() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0;
	}
	return GetOwner()->HasAuthority() ? World->GetTimeSeconds() : World->GetTimeSeconds() + ServerTimeOffset;
}

// ---------------------------------------------------------------- History (server)

void UFPSLagCompensationComponent::RecordFrame()
{
	if (Hitboxes.Num() == 0 || bRewound)
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();

	FFPSHitboxFrame& Frame = History.AddDefaulted_GetRef();
	Frame.Time = Now;
	Frame.Transforms.Reserve(Hitboxes.Num());
	for (const UShapeComponent* Hitbox : Hitboxes)
	{
		Frame.Transforms.Add(Hitbox ? Hitbox->GetComponentTransform() : FTransform::Identity);
	}

	// Drop frames older than MaxHistoryTime (keep at least 2 for blending).
	int32 RemoveCount = 0;
	while (RemoveCount < History.Num() - 2 && Now - History[RemoveCount].Time > MaxHistoryTime)
	{
		++RemoveCount;
	}
	if (RemoveCount > 0)
	{
		History.RemoveAt(0, RemoveCount, EAllowShrinking::No);
	}
}

bool UFPSLagCompensationComponent::GetTransformsAt(double Time, TArray<FTransform>& OutTransforms) const
{
	if (History.Num() == 0)
	{
		return false;
	}
	if (Time >= History.Last().Time)
	{
		OutTransforms = History.Last().Transforms;
		return true;
	}
	if (Time <= History[0].Time)
	{
		OutTransforms = History[0].Transforms;
		return true;
	}

	// Walk back from newest to find the two frames around Time, then blend between them.
	for (int32 i = History.Num() - 1; i > 0; --i)
	{
		const FFPSHitboxFrame& Older = History[i - 1];
		const FFPSHitboxFrame& Newer = History[i];
		if (Older.Time <= Time)
		{
			const float Alpha = static_cast<float>((Time - Older.Time) / FMath::Max(Newer.Time - Older.Time, 1e-6));
			const int32 Num = FMath::Min(Older.Transforms.Num(), Newer.Transforms.Num());
			OutTransforms.SetNum(Num);
			for (int32 j = 0; j < Num; ++j)
			{
				OutTransforms[j].Blend(Older.Transforms[j], Newer.Transforms[j], Alpha);
			}
			return true;
		}
	}
	return false;
}

// ---------------------------------------------------------------- Rewind (server)

void UFPSLagCompensationComponent::Rewind(double Time)
{
	TArray<FTransform> Past;
	if (bRewound || !GetTransformsAt(Time, Past))
	{
		return;
	}

	SavedTransforms.SetNum(Hitboxes.Num());
	for (int32 i = 0; i < Hitboxes.Num(); ++i)
	{
		UShapeComponent* Hitbox = Hitboxes[i];
		if (!Hitbox || !Past.IsValidIndex(i))
		{
			continue;
		}
		SavedTransforms[i] = Hitbox->GetComponentTransform();
		Hitbox->SetWorldTransform(Past[i], false, nullptr, ETeleportType::TeleportPhysics);
	}
	bRewound = true;
}

void UFPSLagCompensationComponent::Restore()
{
	if (!bRewound)
	{
		return;
	}
	for (int32 i = 0; i < Hitboxes.Num(); ++i)
	{
		if (Hitboxes[i] && SavedTransforms.IsValidIndex(i))
		{
			Hitboxes[i]->SetWorldTransform(SavedTransforms[i], false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
	bRewound = false;
}

void UFPSLagCompensationComponent::RewindAll(UWorld* World, double Time, const AActor* IgnoreActor)
{
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		if (*It == IgnoreActor)
		{
			continue;
		}
		if (UFPSLagCompensationComponent* LagComp = It->FindComponentByClass<UFPSLagCompensationComponent>())
		{
			LagComp->Rewind(Time);
		}
	}
}

void UFPSLagCompensationComponent::RestoreAll(UWorld* World)
{
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		if (UFPSLagCompensationComponent* LagComp = It->FindComponentByClass<UFPSLagCompensationComponent>())
		{
			LagComp->Restore();
		}
	}
}

// ---------------------------------------------------------------- Debug

void UFPSLagCompensationComponent::AppendDebugHitboxes(TArray<FFPSDebugHitbox>& Out) const
{
	for (const UShapeComponent* Hitbox : Hitboxes)
	{
		if (!Hitbox || !Hitbox->IsCollisionEnabled())
		{
			continue;
		}

		FFPSDebugHitbox& Box = Out.AddDefaulted_GetRef();
		Box.Location = Hitbox->GetComponentLocation();
		Box.Rotation = Hitbox->GetComponentRotation();

		if (const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Hitbox))
		{
			Box.Radius = Capsule->GetScaledCapsuleRadius();
			Box.HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		}
		else if (const USphereComponent* Sphere = Cast<USphereComponent>(Hitbox))
		{
			Box.Radius = Sphere->GetScaledSphereRadius();
		}
	}
}

void UFPSLagCompensationComponent::GatherDebugHitboxes(UWorld* World, const AActor* IgnoreActor, const FVector& RayStart, const FVector& RayEnd, TArray<FFPSDebugHitbox>& Out)
{
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		if (*It == IgnoreActor)
		{
			continue;
		}
		// Only pawns near the shot, so 50 enemies don't flood the RPC.
		if (FMath::PointDistToSegment(It->GetActorLocation(), RayStart, RayEnd) > 300.f)
		{
			continue;
		}
		if (const UFPSLagCompensationComponent* LagComp = It->FindComponentByClass<UFPSLagCompensationComponent>())
		{
			LagComp->AppendDebugHitboxes(Out);
		}
	}
}

void UFPSLagCompensationComponent::DrawDebugHitbox(UWorld* World, const FFPSDebugHitbox& Box, const FColor& Color, float Duration)
{
	if (Box.HalfHeight > 0.f)
	{
		DrawDebugCapsule(World, Box.Location, Box.HalfHeight, Box.Radius, Box.Rotation.Quaternion(), Color, false, Duration, 0, 0.5f);
	}
	else
	{
		DrawDebugSphere(World, Box.Location, Box.Radius, 12, Color, false, Duration, 0, 0.5f);
	}
}
