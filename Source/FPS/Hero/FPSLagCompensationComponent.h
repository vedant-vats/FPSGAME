#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "FPSLagCompensationComponent.generated.h"

class UShapeComponent;

/** One hitbox shape, sent to the shooter for the debug view. HalfHeight 0 = sphere. */
USTRUCT()
struct FFPSDebugHitbox
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize10 Location;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY()
	float Radius = 0.f;

	UPROPERTY()
	float HalfHeight = 0.f;
};

/** Hitbox transforms at one server time. Same order as Hitboxes. */
struct FFPSHitboxFrame
{
	double Time = 0.0;
	TArray<FTransform> Transforms;
};

/**
 * Server-side rewind (lag compensation).
 *  Server: records hitbox transforms every frame (~1 s). On a shot, moves hitboxes back
 *          to where the shooter saw them, traces, then restores.
 *  Owning client: keeps a synced clock (server time + round trip time).
 * Put on anything shootable (heroes now, enemies in W5).
 */
UCLASS(ClassGroup = (FPS), meta = (BlueprintSpawnableComponent))
class FPS_API UFPSLagCompensationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFPSLagCompensationComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ---- Server: rewind ----
	/** Move hitboxes of every pawn in World (except IgnoreActor) back to Time. Always call RestoreAll after. */
	static void RewindAll(UWorld* World, double Time, const AActor* IgnoreActor);
	static void RestoreAll(UWorld* World);

	// ---- Debug ----
	/** Hitboxes of pawns near the ray, as they are right now (rewound if called between Rewind/Restore). */
	static void GatherDebugHitboxes(UWorld* World, const AActor* IgnoreActor, const FVector& RayStart, const FVector& RayEnd, TArray<FFPSDebugHitbox>& Out);
	static void DrawDebugHitbox(UWorld* World, const FFPSDebugHitbox& Box, const FColor& Color, float Duration);

	const TArray<TObjectPtr<UShapeComponent>>& GetHitboxes() const { return Hitboxes; }

	// ---- Owning client: clock ----
	/** Our estimate of the server's current time. On the server: just its time. */
	double GetServerTime() const;

	/** Round trip time in seconds (0 on the server). */
	float GetRoundTripTime() const { return RoundTripTime; }

protected:
	UPROPERTY(EditDefaultsOnly, Category = "LagComp", meta = (Units = "s"))
	float MaxHistoryTime = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "LagComp", meta = (Units = "s"))
	float ClockSyncInterval = 1.f;

private:
	void CollectHitboxes();
	void RecordFrame();
	bool GetTransformsAt(double Time, TArray<FTransform>& OutTransforms) const;
	void Rewind(double Time);
	void Restore();
	void AppendDebugHitboxes(TArray<FFPSDebugHitbox>& Out) const;

	UFUNCTION(Server, Unreliable)
	void ServerRequestTime(double ClientSendTime);

	UFUNCTION(Client, Unreliable)
	void ClientReceiveTime(double ClientSendTime, double ServerTime);

	/** Shapes on the owner that block the Weapon channel. Found at BeginPlay. */
	UPROPERTY()
	TArray<TObjectPtr<UShapeComponent>> Hitboxes;

	TArray<FFPSHitboxFrame> History;      // server, oldest first
	TArray<FTransform> SavedTransforms;   // server, during a rewind
	bool bRewound = false;

	double ServerTimeOffset = 0.0;        // client
	float RoundTripTime = 0.f;            // client
	float TimeSinceSync = 0.f;            // client
	bool bHasClockSync = false;           // client
};
