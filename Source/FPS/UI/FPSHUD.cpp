#include "FPSHUD.h"
#include "GAS/FPSAttributeSet.h"
#include "GAS/FPSGameplayAbility.h"
#include "Core/FPSGameplayTags.h"
#include "Core/FPSGameState.h"
#include "Enemies/FPSEnemyCharacter.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Blueprint/UserWidget.h"
#include "Hero/FPSHeroCharacter.h"
#include "Weapons/FPSWeaponComponent.h"

#include "AbilitySystemComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"

void AFPSHUD::BeginPlay()
{
	Super::BeginPlay();

	if (WaveWidgetClass && PlayerOwner && PlayerOwner->IsLocalController())
	{
		WaveWidget = CreateWidget<UUserWidget>(PlayerOwner, WaveWidgetClass);
		if (WaveWidget)
		{
			WaveWidget->AddToViewport(10);
		}
	}
}

void AFPSHUD::DrawHUD()
{
	Super::DrawHUD();

	const AFPSHeroCharacter* Hero = Cast<AFPSHeroCharacter>(GetOwningPawn());
	if (!Canvas || !Hero || !PlayerOwner)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	const float DeltaSeconds = FMath::Clamp(Now - LastDrawTime, 0.f, 0.1f);
	LastDrawTime = Now;

	DrawEnemyBars();
	DrawBars(Hero, DeltaSeconds);
	DrawMatchInfo(Hero);

	if (Hero->IsDead())
	{
		return;
	}

	DrawAbilities(Hero);
	if (const UFPSWeaponComponent* Weapon = Hero->FindComponentByClass<UFPSWeaponComponent>())
	{
		DrawCrosshair(Weapon);
	}

	if (Hero->IsStunned())
	{
		DrawText(TEXT("STUNNED"), FLinearColor::Yellow, Canvas->ClipX * 0.5f - 40.f, Canvas->ClipY * 0.5f + 40.f, GEngine->GetMediumFont(), 1.2f);
	}
}

// ---------------------------------------------------------------- Crosshair

void AFPSHUD::DrawCrosshair(const UFPSWeaponComponent* Weapon)
{
	if (!PlayerOwner->PlayerCameraManager)
	{
		return;
	}

	const float CenterX = Canvas->ClipX * 0.5f;
	const float CenterY = Canvas->ClipY * 0.5f;

	// Spread angle -> pixels: where a bullet at the cone edge lands on screen.
	const float HalfFovRad = FMath::DegreesToRadians(PlayerOwner->PlayerCameraManager->GetFOVAngle() * 0.5f);
	const float SpreadRad = FMath::DegreesToRadians(Weapon->GetCurrentSpread());
	const float Gap = MinGap + FMath::Tan(SpreadRad) / FMath::Tan(HalfFovRad) * CenterX;

	DrawLine(CenterX - Gap - LineLength, CenterY, CenterX - Gap, CenterY, CrosshairColor, LineThickness);
	DrawLine(CenterX + Gap, CenterY, CenterX + Gap + LineLength, CenterY, CrosshairColor, LineThickness);
	DrawLine(CenterX, CenterY - Gap - LineLength, CenterX, CenterY - Gap, CrosshairColor, LineThickness);
	DrawLine(CenterX, CenterY + Gap, CenterX, CenterY + Gap + LineLength, CrosshairColor, LineThickness);
	DrawRect(CrosshairColor, CenterX - 1.f, CenterY - 1.f, 2.f, 2.f);

	// Hit marker: only for hits the SERVER confirmed.
	if (GetWorld()->GetTimeSeconds() - Weapon->GetLastHitTime() < HitMarkerDuration)
	{
		const FLinearColor MarkerColor = Weapon->WasLastHitHeadshot() ? FLinearColor::Red : FLinearColor::White;
		const float In = Gap + 2.f;
		const float Out = In + HitMarkerSize;
		DrawLine(CenterX - Out, CenterY - Out, CenterX - In, CenterY - In, MarkerColor, LineThickness);
		DrawLine(CenterX + In, CenterY - In, CenterX + Out, CenterY - Out, MarkerColor, LineThickness);
		DrawLine(CenterX - Out, CenterY + Out, CenterX - In, CenterY + In, MarkerColor, LineThickness);
		DrawLine(CenterX + In, CenterY + In, CenterX + Out, CenterY + Out, MarkerColor, LineThickness);
	}
}

// ---------------------------------------------------------------- Health / shield

void AFPSHUD::DrawBar(float X, float Y, float W, float H, float Fill, float Ghost, const FLinearColor& Color)
{
	DrawRect(BackColor, X, Y, W, H);
	if (Ghost > Fill)
	{
		DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.6f), X, Y, W * Ghost, H); // damage just taken
	}
	DrawRect(Color, X, Y, W * Fill, H);
}

void AFPSHUD::DrawBars(const AFPSHeroCharacter* Hero, float DeltaSeconds)
{
	const UFPSAttributeSet* AS = Hero->GetAttributeSet();
	if (!AS)
	{
		return;
	}

	const float TargetHealth = AS->GetMaxHealth() > 0.f ? AS->GetHealth() / AS->GetMaxHealth() : 0.f;
	const float TargetShield = AS->GetMaxShield() > 0.f ? AS->GetShield() / AS->GetMaxShield() : 0.f;

	// Bar slides fast, ghost slides slow -> you see the chunk you lost.
	ShownHealth = FMath::FInterpTo(ShownHealth, TargetHealth, DeltaSeconds, BarSmoothSpeed);
	ShownShield = FMath::FInterpTo(ShownShield, TargetShield, DeltaSeconds, BarSmoothSpeed);
	GhostHealth = TargetHealth > GhostHealth ? TargetHealth : FMath::FInterpTo(GhostHealth, TargetHealth, DeltaSeconds, GhostSpeed);
	GhostShield = TargetShield > GhostShield ? TargetShield : FMath::FInterpTo(GhostShield, TargetShield, DeltaSeconds, GhostSpeed);

	const float Scale = Canvas->ClipY / 1080.f;
	const float X = 40.f * Scale;
	const float W = 360.f * Scale;
	const float ShieldH = 10.f * Scale;
	const float HealthH = 18.f * Scale;
	const float HealthY = Canvas->ClipY - 60.f * Scale;
	const float ShieldY = HealthY - ShieldH - 4.f * Scale;

	DrawBar(X, ShieldY, W, ShieldH, ShownShield, GhostShield, ShieldColor);

	// Health turns red when low.
	const FLinearColor Health = TargetHealth < 0.3f ? FLinearColor(1.f, 0.25f, 0.2f, 0.95f) : HealthColor;
	DrawBar(X, HealthY, W, HealthH, ShownHealth, GhostHealth, Health);

	const FString Text = FString::Printf(TEXT("%.0f  |  %.0f"), AS->GetHealth(), AS->GetShield());
	DrawText(Text, FLinearColor::White, X + W + 12.f * Scale, ShieldY, GEngine->GetMediumFont(), Scale * 1.2f);
}

// ---------------------------------------------------------------- Abilities

void AFPSHUD::DrawAbilities(const AFPSHeroCharacter* Hero)
{
	UAbilitySystemComponent* ASC = Hero->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	const float Scale = Canvas->ClipY / 1080.f;
	const float Size = 64.f * Scale;
	const float Spacing = 12.f * Scale;
	float X = Canvas->ClipX - 40.f * Scale - Size;
	const float Y = Canvas->ClipY - 60.f * Scale - Size + 18.f * Scale;
	UFont* Font = GEngine->GetMediumFont();

	// Right to left: area attack, then dash.
	const FGameplayTag Order[] = { FPSTags::InputTag_Ability_Area, FPSTags::InputTag_Ability_Dash };
	for (const FGameplayTag& Tag : Order)
	{
		const UFPSGameplayAbility* Ability = nullptr;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			const UFPSGameplayAbility* Candidate = Cast<UFPSGameplayAbility>(Spec.Ability);
			if (Candidate && Candidate->InputTag.MatchesTagExact(Tag))
			{
				Ability = Candidate;
				break;
			}
		}
		if (!Ability)
		{
			continue;
		}

		float Remaining = 0.f;
		float Duration = 0.f;
		int32 ChargesReady = 0;
		Ability->GetCooldownState(ASC, Remaining, Duration, ChargesReady);
		const bool bReady = ChargesReady > 0;

		// Box + cooldown fill rising from the bottom.
		DrawRect(BackColor, X, Y, Size, Size);
		if (Remaining > 0.f && Duration > 0.f)
		{
			const float Progress = 1.f - Remaining / Duration;
			const float FillH = Size * Progress;
			DrawRect(bReady ? AbilityReadyColor * 0.6f : AbilityCooldownColor, X, Y + Size - FillH, Size, FillH);
		}
		else
		{
			DrawRect(AbilityReadyColor * FLinearColor(1.f, 1.f, 1.f, 0.35f), X, Y, Size, Size);
		}

		// Border: bright when usable.
		const FLinearColor Border = bReady ? AbilityReadyColor : AbilityCooldownColor;
		DrawLine(X, Y, X + Size, Y, Border, 2.f);
		DrawLine(X, Y + Size, X + Size, Y + Size, Border, 2.f);
		DrawLine(X, Y, X, Y + Size, Border, 2.f);
		DrawLine(X + Size, Y, X + Size, Y + Size, Border, 2.f);

		// Key on top, seconds left in the middle (only when nothing is ready).
		DrawText(Ability->HUDKey.ToString(), FLinearColor::White, X + 4.f * Scale, Y + 2.f * Scale, Font, Scale);
		if (!bReady && Remaining > 0.f)
		{
			DrawText(FString::Printf(TEXT("%.1f"), Remaining), FLinearColor::White, X + Size * 0.3f, Y + Size * 0.35f, Font, Scale * 1.4f);
		}

		// Charge pips under the box.
		if (Ability->MaxCharges > 1)
		{
			const float Pip = 10.f * Scale;
			for (int32 i = 0; i < Ability->MaxCharges; ++i)
			{
				const FLinearColor PipColor = i < ChargesReady ? AbilityReadyColor : AbilityCooldownColor;
				DrawRect(PipColor, X + i * (Pip + 4.f * Scale), Y + Size + 4.f * Scale, Pip, Pip * 0.5f);
			}
		}

		DrawText(Ability->HUDName.ToString(), FLinearColor(1.f, 1.f, 1.f, 0.8f), X, Y - 22.f * Scale, Font, Scale * 0.9f);

		X -= Size + Spacing;
	}
}

// ---------------------------------------------------------------- Waves / match

void AFPSHUD::DrawCenteredText(const FString& Text, float Y, const FLinearColor& Color, float Scale)
{
	UFont* Font = GEngine->GetLargeFont();
	float W = 0.f;
	float H = 0.f;
	GetTextSize(Text, W, H, Font, Scale);
	DrawText(Text, Color, (Canvas->ClipX - W) * 0.5f, Y, Font, Scale);
}

void AFPSHUD::DrawMatchInfo(const AFPSHeroCharacter* Hero)
{
	const AFPSGameState* GS = GetWorld()->GetGameState<AFPSGameState>();
	if (!GS)
	{
		return;
	}

	const float Scale = Canvas->ClipY / 1080.f;
	const float TopY = 30.f * Scale;
	const int32 Seconds = FMath::CeilToInt(GS->GetPhaseTimeRemaining());

	// Fallback only: the UMG wave widget takes over when it's set.
	if (!WaveWidget)
	{
	switch (GS->Phase)
	{
	case EFPSMatchPhase::WaitingToStart:
		DrawCenteredText(FString::Printf(TEXT("FIRST WAVE IN %d"), Seconds), TopY, FLinearColor::White, Scale);
		break;
	case EFPSMatchPhase::InWave:
		DrawCenteredText(FString::Printf(TEXT("WAVE %d / %d"), GS->CurrentWave, GS->TotalWaves), TopY, FLinearColor::White, Scale);
		DrawCenteredText(FString::Printf(TEXT("%d ENEMIES LEFT"), GS->EnemiesRemaining), TopY + 34.f * Scale, FLinearColor(1.f, 0.6f, 0.2f), Scale * 0.8f);
		break;
	case EFPSMatchPhase::Intermission:
		DrawCenteredText(FString::Printf(TEXT("WAVE %d CLEARED"), GS->CurrentWave), TopY, FLinearColor(0.3f, 1.f, 0.4f), Scale);
		DrawCenteredText(FString::Printf(TEXT("NEXT WAVE IN %d"), Seconds), TopY + 34.f * Scale, FLinearColor::White, Scale * 0.8f);
		break;
	case EFPSMatchPhase::Victory:
		DrawCenteredText(TEXT("VICTORY"), Canvas->ClipY * 0.35f, FLinearColor(0.3f, 1.f, 0.4f), Scale * 2.5f);
		DrawCenteredText(FString::Printf(TEXT("Restarting in %d"), Seconds), Canvas->ClipY * 0.35f + 70.f * Scale, FLinearColor::White, Scale);
		break;
	case EFPSMatchPhase::Defeat:
		DrawCenteredText(TEXT("DEFEAT"), Canvas->ClipY * 0.35f, FLinearColor(1.f, 0.25f, 0.2f), Scale * 2.5f);
		DrawCenteredText(FString::Printf(TEXT("Restarting in %d"), Seconds), Canvas->ClipY * 0.35f + 70.f * Scale, FLinearColor::White, Scale);
		break;
	}
	}

	// Local player respawn countdown.
	if (Hero->IsDead() && !GS->IsMatchOver())
	{
		const int32 RespawnIn = FMath::CeilToInt(FMath::Max(0.f, Hero->GetRespawnServerTime() - static_cast<float>(GS->GetServerWorldTimeSeconds())));
		DrawCenteredText(FString::Printf(TEXT("RESPAWN IN %d"), RespawnIn), Canvas->ClipY * 0.45f, FLinearColor(1.f, 0.4f, 0.3f), Scale * 1.5f);
	}
}

// ---------------------------------------------------------------- Enemy health bars

void AFPSHUD::DrawEnemyBars()
{
	// One HUD pass for every enemy: no widget component per enemy (50 widgets = 50 ticks + draws).
	const APlayerCameraManager* Camera = PlayerOwner->PlayerCameraManager;
	if (!Camera)
	{
		return;
	}

	const FVector CameraLocation = Camera->GetCameraLocation();
	const FVector CameraForward = Camera->GetCameraRotation().Vector();
	const float MaxDistSq = EnemyBarMaxDistance * EnemyBarMaxDistance;
	const float UIScale = Canvas->ClipY / 1080.f;
	UFont* Font = GEngine->GetSmallFont();

	for (TActorIterator<AFPSEnemyCharacter> It(GetWorld()); It; ++It)
	{
		const AFPSEnemyCharacter* Enemy = *It;
		// Only elites and the mini-boss get a bar + name (normal enemies = clutter in a horde).
		if (Enemy->IsHidden() || Enemy->IsCombatantDead() || Enemy->GetTier() == 0)
		{
			continue;
		}

		// Cheap rejects first: too far, or behind the camera.
		const FVector ToEnemy = Enemy->GetActorLocation() - CameraLocation;
		const float DistSq = ToEnemy.SizeSquared();
		if (DistSq > MaxDistSq || FVector::DotProduct(ToEnemy, CameraForward) <= 0.f)
		{
			continue;
		}

		const UFPSAttributeSet* AS = Enemy->GetAttributeSet();
		if (!AS || AS->GetMaxHealth() <= 0.f)
		{
			continue;
		}

		// Point just above the head (capsule already includes elite/boss scale).
		const FVector Above = Enemy->GetActorLocation() + FVector(0.f, 0.f, Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 25.f);
		const FVector Screen = Project(Above);
		if (Screen.Z <= 0.f || Screen.X < 0.f || Screen.X > Canvas->ClipX || Screen.Y < 0.f || Screen.Y > Canvas->ClipY)
		{
			continue;
		}

		// Shrink a bit with distance so far bars don't clutter.
		const float Dist = FMath::Sqrt(DistSq);
		const float DistScale = FMath::GetMappedRangeValueClamped(FVector2D(500.f, EnemyBarMaxDistance), FVector2D(1.f, 0.6f), Dist);
		const uint8 Tier = Enemy->GetTier();
		const float TierScale = Tier == 2 ? 2.2f : (Tier == 1 ? 1.4f : 1.f);

		const float W = EnemyBarWidth * TierScale * DistScale * UIScale;
		const float H = FMath::Max(3.f, 6.f * DistScale * UIScale * (Tier == 2 ? 1.5f : 1.f));
		const float X = Screen.X - W * 0.5f;
		const float Y = Screen.Y;
		const float Fill = FMath::Clamp(AS->GetHealth() / AS->GetMaxHealth(), 0.f, 1.f);

		DrawRect(BackColor, X - 1.f, Y - 1.f, W + 2.f, H + 2.f);
		DrawRect(EnemyHealthColor, X, Y, W * Fill, H);

		// Name: white / orange elite / red mini-boss.
		FString Name = Enemy->GetDisplayName().ToString();
		FLinearColor NameColor = FLinearColor(1.f, 1.f, 1.f, 0.85f);
		if (Tier == 1)
		{
			Name = TEXT("Elite ") + Name;
			NameColor = FLinearColor(1.f, 0.6f, 0.15f);
		}
		else if (Tier == 2)
		{
			Name = TEXT("Mini-Boss ") + Name;
			NameColor = FLinearColor(1.f, 0.25f, 0.2f);
		}

		const float TextScale = DistScale * UIScale * (Tier > 0 ? 1.1f : 0.9f);
		float TW = 0.f;
		float TH = 0.f;
		GetTextSize(Name, TW, TH, Font, TextScale);
		DrawText(Name, NameColor, Screen.X - TW * 0.5f, Y - TH - 2.f, Font, TextScale);
	}
}
